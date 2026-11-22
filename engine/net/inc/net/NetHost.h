#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

typedef struct _ENetHost ENetHost;

namespace Net
{
	// Opaque handle of a remote peer. Valid from the Connected event until the Disconnected event.
	using PeerHandle = uintptr_t;
	constexpr PeerHandle InvalidPeer = 0;

	struct NetEvent
	{
		enum class Type
		{
			None,
			Connected,
			Disconnected,
			Data,
		};

		Type type = Type::None;
		PeerHandle peer = InvalidPeer;
		uint32_t data = 0;            // Connected: user data passed to Connect(); Disconnected: reason code (0 = timeout)
		std::vector<uint8_t> payload; // Data only
	};

	// Thin wrapper around an ENet host. Not thread safe; all calls including Pump()
	// are expected to come from the main loop. All data is sent reliably and in order.
	class NetHost final
	{
	public:
		// Throws std::runtime_error if the socket could not be bound.
		static std::unique_ptr<NetHost> CreateServer(uint16_t port, size_t maxPeers);
		static std::unique_ptr<NetHost> CreateClient();
		~NetHost();

		// Starts an asynchronous connection attempt. The result arrives later as a
		// Connected or Disconnected event. Returns InvalidPeer if the host name could not be resolved.
		PeerHandle Connect(std::string_view hostname, uint16_t port, uint32_t userData = 0);

		void Send(PeerHandle peer, const void *data, size_t size);
		void Send(PeerHandle peer, const std::vector<uint8_t> &data) { Send(peer, data.data(), data.size()); }
		void Broadcast(const void *data, size_t size);
		void Broadcast(const std::vector<uint8_t> &data) { Broadcast(data.data(), data.size()); }

		// Graceful disconnect; a Disconnected event follows once the peer acknowledges it.
		void Disconnect(PeerHandle peer, uint32_t reason);
		// Immediate disconnect; no event is generated.
		void DisconnectNow(PeerHandle peer);

		// Processes one pending event without blocking. Returns false when there is nothing to process.
		bool Pump(NetEvent &outEvent);
		// Sends queued packets without waiting for events.
		void Flush();

		// How long a silent peer is kept before it is considered gone (ENet default is 5-30 s).
		// New outgoing connections get c_defaultTimeoutMs automatically; call this for accepted peers.
		void SetTimeout(PeerHandle peer, uint32_t minimumMs, uint32_t maximumMs);
		static constexpr uint32_t c_defaultTimeoutMinMs = 3000;
		static constexpr uint32_t c_defaultTimeoutMaxMs = 8000;

		uint32_t GetRoundTripTime(PeerHandle peer) const; // milliseconds
		size_t GetPeerCount() const;   // fully connected peers
		bool HasActivePeers() const;   // peers in any state but disconnected, e.g. still disconnecting
		uint16_t GetPort() const;

		NetHost(const NetHost&) = delete;
		NetHost& operator=(const NetHost&) = delete;

	private:
		explicit NetHost(ENetHost *host);
		ENetHost *_host;
	};
}
