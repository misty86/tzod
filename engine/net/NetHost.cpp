#include "inc/net/NetHost.h"
#include <enet/enet.h>
#include <cassert>
#include <stdexcept>
#include <string>

using namespace Net;

namespace
{
	constexpr size_t c_channelCount = 1;

	// enet_initialize() must be called once per process while any host is alive
	class EnetLibrary
	{
	public:
		static void AddRef()
		{
			if (0 == s_refCount++)
			{
				if (0 != enet_initialize())
					throw std::runtime_error("could not initialize ENet");
			}
		}
		static void Release()
		{
			assert(s_refCount > 0);
			if (0 == --s_refCount)
				enet_deinitialize();
		}
	private:
		static int s_refCount;
	};
	int EnetLibrary::s_refCount = 0;

	ENetPeer* ToPeer(PeerHandle handle)
	{
		return reinterpret_cast<ENetPeer*>(handle);
	}

	PeerHandle ToHandle(ENetPeer *peer)
	{
		return reinterpret_cast<PeerHandle>(peer);
	}
}

NetHost::NetHost(ENetHost *host)
	: _host(host)
{
	assert(_host);
}

NetHost::~NetHost()
{
	enet_host_destroy(_host);
	EnetLibrary::Release();
}

std::unique_ptr<NetHost> NetHost::CreateServer(uint16_t port, size_t maxPeers)
{
	EnetLibrary::AddRef();
	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = port;
	ENetHost *host = enet_host_create(&address, maxPeers, c_channelCount, 0 /*incoming bandwidth*/, 0 /*outgoing bandwidth*/);
	if (!host)
	{
		EnetLibrary::Release();
		throw std::runtime_error("could not bind port " + std::to_string(port));
	}
	return std::unique_ptr<NetHost>(new NetHost(host));
}

std::unique_ptr<NetHost> NetHost::CreateClient()
{
	EnetLibrary::AddRef();
	ENetHost *host = enet_host_create(nullptr /*any address*/, 1 /*one outgoing connection*/, c_channelCount, 0, 0);
	if (!host)
	{
		EnetLibrary::Release();
		throw std::runtime_error("could not create client socket");
	}
	return std::unique_ptr<NetHost>(new NetHost(host));
}

PeerHandle NetHost::Connect(std::string_view hostname, uint16_t port, uint32_t userData)
{
	ENetAddress address;
	address.port = port;
	if (0 != enet_address_set_host(&address, std::string(hostname).c_str()))
		return InvalidPeer;

	ENetPeer *peer = enet_host_connect(_host, &address, c_channelCount, userData);
	if (!peer)
		return InvalidPeer;
	enet_peer_timeout(peer, 0 /*default limit*/, c_defaultTimeoutMinMs, c_defaultTimeoutMaxMs);
	return ToHandle(peer);
}

void NetHost::SetTimeout(PeerHandle peer, uint32_t minimumMs, uint32_t maximumMs)
{
	assert(peer != InvalidPeer);
	enet_peer_timeout(ToPeer(peer), 0 /*default limit*/, minimumMs, maximumMs);
}

void NetHost::Send(PeerHandle peer, const void *data, size_t size)
{
	assert(peer != InvalidPeer);
	ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
	if (!packet || 0 != enet_peer_send(ToPeer(peer), 0 /*channel*/, packet))
	{
		// peer is not connected; the packet is destroyed by enet_peer_send on failure
	}
}

void NetHost::Broadcast(const void *data, size_t size)
{
	ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
	if (packet)
		enet_host_broadcast(_host, 0 /*channel*/, packet);
}

void NetHost::Disconnect(PeerHandle peer, uint32_t reason)
{
	assert(peer != InvalidPeer);
	enet_peer_disconnect(ToPeer(peer), reason);
}

void NetHost::DisconnectNow(PeerHandle peer)
{
	assert(peer != InvalidPeer);
	enet_peer_disconnect_now(ToPeer(peer), 0);
}

bool NetHost::Pump(NetEvent &outEvent)
{
	ENetEvent event;
	int result = enet_host_service(_host, &event, 0 /*do not wait*/);
	if (result <= 0)
	{
		outEvent.type = NetEvent::Type::None;
		return false;
	}

	outEvent.peer = ToHandle(event.peer);
	outEvent.data = event.data;
	outEvent.payload.clear();

	switch (event.type)
	{
	case ENET_EVENT_TYPE_CONNECT:
		outEvent.type = NetEvent::Type::Connected;
		break;

	case ENET_EVENT_TYPE_DISCONNECT:
		outEvent.type = NetEvent::Type::Disconnected;
		break;

	case ENET_EVENT_TYPE_RECEIVE:
		outEvent.type = NetEvent::Type::Data;
		outEvent.payload.assign(event.packet->data, event.packet->data + event.packet->dataLength);
		enet_packet_destroy(event.packet);
		break;

	default:
		outEvent.type = NetEvent::Type::None;
		return false;
	}
	return true;
}

void NetHost::Flush()
{
	enet_host_flush(_host);
}

uint32_t NetHost::GetRoundTripTime(PeerHandle peer) const
{
	assert(peer != InvalidPeer);
	return ToPeer(peer)->roundTripTime;
}

size_t NetHost::GetPeerCount() const
{
	size_t count = 0;
	for (size_t i = 0; i < _host->peerCount; ++i)
	{
		if (_host->peers[i].state == ENET_PEER_STATE_CONNECTED)
			++count;
	}
	return count;
}

bool NetHost::HasActivePeers() const
{
	for (size_t i = 0; i < _host->peerCount; ++i)
	{
		if (_host->peers[i].state != ENET_PEER_STATE_DISCONNECTED)
			return true;
	}
	return false;
}

uint16_t NetHost::GetPort() const
{
	return _host->address.port;
}
