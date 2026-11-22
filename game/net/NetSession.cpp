#include "inc/net/NetSession.h"
#include <plat/ConsoleBuffer.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <thread>

using namespace Net;

namespace
{
	// Gives ENet a moment to deliver pending disconnect notifications before the socket goes away
	void GracefulShutdown(NetHost &host)
	{
		host.Flush(); // get the disconnect commands on the wire
		for (int i = 0; i < 20 && host.HasActivePeers(); ++i)
		{
			NetEvent ev;
			while (host.Pump(ev)) {}
			host.Flush();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

HostSession::HostSession(uint16_t port, NetPlayerDesc hostPlayer, Plat::ConsoleBuffer &logger)
	: _host(NetHost::CreateServer(port, MAX_NET_PLAYERS))
	, _logger(logger)
{
	_lobby.humans.push_back(std::move(hostPlayer));
	_logger.Printf(0, "net: listening on port %u", (unsigned)port);
}

HostSession::~HostSession()
{
	for (auto &client : _clients)
		_host->Disconnect(client.peer, static_cast<uint32_t>(DisconnectReason::HostLeft));
	_clients.clear();
	GracefulShutdown(*_host);
}

void HostSession::SetMap(std::string mapName, uint32_t mapHash)
{
	_lobby.mapName = std::move(mapName);
	_lobby.mapHash = mapHash;
	BroadcastLobby();
}

void HostSession::AddBot(NetPlayerDesc bot)
{
	if (_lobby.humans.size() + _lobby.bots.size() < MAX_NET_PLAYERS)
	{
		_lobby.bots.push_back(std::move(bot));
		BroadcastLobby();
	}
}

void HostSession::RemoveBot(unsigned botIndex)
{
	if (botIndex < _lobby.bots.size())
	{
		_lobby.bots.erase(_lobby.bots.begin() + botIndex);
		BroadcastLobby();
	}
}

void HostSession::SetRules(uint8_t difficulty, int fragLimit, float timeLimit)
{
	_lobby.difficulty = difficulty;
	_lobby.fragLimit = fragLimit;
	_lobby.timeLimit = timeLimit;
	BroadcastLobby();
}

StartGameMsg HostSession::StartGame(uint32_t seed, float tickRate, uint8_t inputDelay, bool debugChecksum)
{
	assert(!_started);
	_started = true;

	// anybody who has not finished the handshake yet is out
	for (size_t i = _clients.size(); i-- > 0;)
	{
		if (_clients[i].playerIndex < 0)
			Reject(i, RejectReason::GameAlreadyStarted, "Game already started");
	}

	StartGameMsg msg;
	msg.lobby = _lobby;
	msg.seed = seed;
	msg.tickRate = tickRate;
	msg.inputDelay = inputDelay;
	msg.debugChecksum = debugChecksum;
	SendToPlayers(Encode(msg));
	_host->Flush();

	_logger.Printf(0, "net: starting game on '%s', %u humans, %u bots, seed %u, tick rate %g, input delay %u",
		_lobby.mapName.c_str(), (unsigned)_lobby.humans.size(), (unsigned)_lobby.bots.size(), seed, tickRate, (unsigned)inputDelay);
	return msg;
}

void HostSession::BroadcastBatch(const InputBatchMsg &batch)
{
	assert(_started);
	_host->Broadcast(Encode(batch));
	_host->Flush(); // do not wait for the next Pump(); every tick counts
}

std::vector<unsigned> HostSession::GetConnectedPlayerIndices() const
{
	std::vector<unsigned> result;
	result.push_back(0);
	for (auto &client : _clients)
	{
		if (client.playerIndex >= 0)
			result.push_back(static_cast<unsigned>(client.playerIndex));
	}
	return result;
}

uint32_t HostSession::GetMaxRoundTripTime() const
{
	uint32_t rtt = 0;
	for (auto &client : _clients)
	{
		if (client.playerIndex >= 0)
			rtt = std::max(rtt, _host->GetRoundTripTime(client.peer));
	}
	return rtt;
}

uint16_t HostSession::GetPort() const
{
	return _host->GetPort();
}

void HostSession::Pump()
{
	NetEvent ev;
	while (_host->Pump(ev))
	{
		switch (ev.type)
		{
		case NetEvent::Type::Connected:
			_host->SetTimeout(ev.peer, NetHost::c_defaultTimeoutMinMs, NetHost::c_defaultTimeoutMaxMs);
			_clients.push_back(Client{ ev.peer, -1 });
			if (_started)
				Reject(_clients.size() - 1, RejectReason::GameAlreadyStarted, "Game already started");
			break;

		case NetEvent::Type::Disconnected:
			if (int index = FindClient(ev.peer); index >= 0)
				RemoveClient(static_cast<size_t>(index));
			break;

		case NetEvent::Type::Data:
			if (int index = FindClient(ev.peer); index >= 0)
				HandleMessage(static_cast<size_t>(index), ev.payload);
			break;

		default:
			break;
		}
	}
}

int HostSession::FindClient(PeerHandle peer) const
{
	for (size_t i = 0; i < _clients.size(); ++i)
	{
		if (_clients[i].peer == peer)
			return static_cast<int>(i);
	}
	return -1;
}

void HostSession::HandleMessage(size_t clientIndex, const std::vector<uint8_t> &packet)
{
	NetReader reader(packet);
	MsgType type = static_cast<MsgType>(reader.U8());
	Client &client = _clients[clientIndex];

	if (client.playerIndex < 0)
	{
		// handshake
		HelloMsg hello;
		if (type != MsgType::Hello || !Decode(reader, hello))
		{
			Reject(clientIndex, RejectReason::BadMessage, "Bad handshake");
			return;
		}
		if (hello.protocolVersion != NET_PROTOCOL_VERSION)
		{
			Reject(clientIndex, RejectReason::VersionMismatch, "Incompatible game version");
			return;
		}
		if (_started)
		{
			Reject(clientIndex, RejectReason::GameAlreadyStarted, "Game already started");
			return;
		}
		if (_lobby.humans.size() + _lobby.bots.size() >= MAX_NET_PLAYERS)
		{
			Reject(clientIndex, RejectReason::ServerFull, "Server is full");
			return;
		}

		client.playerIndex = static_cast<int>(_lobby.humans.size());
		_lobby.humans.push_back(hello.player);

		WelcomeMsg welcome;
		welcome.yourPlayerIndex = static_cast<uint8_t>(client.playerIndex);
		welcome.lobby = _lobby;
		_host->Send(client.peer, Encode(welcome));
		BroadcastLobby(); // the new client gets the lobby twice, that's fine

		_logger.Printf(0, "net: '%s' joined as player %d", hello.player.nick.c_str(), client.playerIndex);
		if (_handler)
		{
			_handler->OnPlayerJoined(static_cast<unsigned>(client.playerIndex), hello.player.nick);
			_handler->OnLobbyChanged(_lobby);
		}
		return;
	}

	switch (type)
	{
	case MsgType::Input:
	{
		InputMsg input;
		if (Decode(reader, input) && _started && _handler)
			_handler->OnInput(static_cast<unsigned>(client.playerIndex), input);
		break;
	}
	case MsgType::Ready:
	{
		ReadyMsg ready;
		if (Decode(reader, ready) && !_started && client.playerIndex < 32)
		{
			uint32_t bit = 1u << client.playerIndex;
			_lobby.readyMask = ready.ready ? (_lobby.readyMask | bit) : (_lobby.readyMask & ~bit);
			BroadcastLobby();
			if (_handler)
				_handler->OnLobbyChanged(_lobby);
		}
		break;
	}
	default:
		_logger.Printf(1, "net: unexpected message %u from player %d", (unsigned)type, client.playerIndex);
		break;
	}
}

void HostSession::RemoveClient(size_t clientIndex)
{
	Client client = _clients[clientIndex];
	_clients.erase(_clients.begin() + clientIndex);
	if (client.playerIndex < 0)
		return;

	unsigned playerIndex = static_cast<unsigned>(client.playerIndex);
	std::string nick = _lobby.humans[playerIndex].nick;
	_logger.Printf(0, "net: '%s' (player %u) left", nick.c_str(), playerIndex);

	if (!_started)
	{
		_lobby.humans.erase(_lobby.humans.begin() + playerIndex);
		for (auto &other : _clients)
		{
			if (other.playerIndex > client.playerIndex)
				--other.playerIndex;
		}
		// shift the ready bits above the removed player down
		uint32_t below = _lobby.readyMask & ((1u << playerIndex) - 1);
		uint32_t above = playerIndex < 31 ? (_lobby.readyMask >> (playerIndex + 1)) << playerIndex : 0;
		_lobby.readyMask = below | above;
		BroadcastLobby();
		if (_handler)
		{
			_handler->OnPlayerLeft(playerIndex, nick);
			_handler->OnLobbyChanged(_lobby);
		}
	}
	else if (_handler)
	{
		_handler->OnPlayerLeft(playerIndex, nick);
	}
}

void HostSession::Reject(size_t clientIndex, RejectReason reason, std::string text)
{
	Client client = _clients[clientIndex];
	_logger.Printf(0, "net: rejecting peer: %s", text.c_str());
	RejectMsg msg;
	msg.reason = reason;
	msg.text = std::move(text);
	_host->Send(client.peer, Encode(msg));
	_host->Disconnect(client.peer, static_cast<uint32_t>(DisconnectReason::Rejected));
	// forget it right away so that the pending Disconnected event is ignored
	_clients.erase(_clients.begin() + clientIndex);
}

void HostSession::BroadcastLobby()
{
	if (!_started)
		SendToPlayers(Encode(_lobby));
}

void HostSession::SendToPlayers(const std::vector<uint8_t> &packet)
{
	for (auto &client : _clients)
	{
		if (client.playerIndex >= 0)
			_host->Send(client.peer, packet);
	}
}

///////////////////////////////////////////////////////////////////////////////

ClientSession::ClientSession(std::string_view hostname, uint16_t port, NetPlayerDesc me, Plat::ConsoleBuffer &logger)
	: _host(NetHost::CreateClient())
	, _me(std::move(me))
	, _logger(logger)
	, _status("Connecting...")
{
	_server = _host->Connect(hostname, port);
	if (InvalidPeer == _server)
		throw std::runtime_error("could not resolve host name '" + std::string(hostname) + "'");
	_logger.Printf(0, "net: connecting to %s:%u", std::string(hostname).c_str(), (unsigned)port);
}

ClientSession::~ClientSession()
{
	if (_server != InvalidPeer && _state != State::Disconnected)
	{
		_host->Disconnect(_server, static_cast<uint32_t>(DisconnectReason::ClientLeft));
		GracefulShutdown(*_host);
	}
}

void ClientSession::SendInput(const InputMsg &msg)
{
	if (_state == State::InGame)
	{
		_host->Send(_server, Encode(msg));
		_host->Flush(); // do not wait for the next Pump(); every tick counts
	}
}

void ClientSession::SetReady(bool ready)
{
	if (_state == State::InLobby && ready != IsReady())
	{
		ReadyMsg msg;
		msg.ready = ready;
		_host->Send(_server, Encode(msg));
		_host->Flush();
	}
}

uint32_t ClientSession::GetRoundTripTime() const
{
	return (_server != InvalidPeer && _state != State::Disconnected) ? _host->GetRoundTripTime(_server) : 0;
}

void ClientSession::Pump()
{
	NetEvent ev;
	while (_host->Pump(ev))
	{
		switch (ev.type)
		{
		case NetEvent::Type::Connected:
		{
			HelloMsg hello;
			hello.player = _me;
			_host->Send(_server, Encode(hello));
			_state = State::Handshaking;
			_status = "Connected, waiting for the host...";
			break;
		}

		case NetEvent::Type::Disconnected:
			if (_state != State::Disconnected)
			{
				_disconnectReason = static_cast<DisconnectReason>(ev.data);
				if (_state == State::Connecting)
					SetDisconnected("Connection failed");
				else if (_disconnectReason == DisconnectReason::HostLeft)
					SetDisconnected("Host left the game");
				else
					SetDisconnected("Connection lost");
			}
			_server = InvalidPeer;
			break;

		case NetEvent::Type::Data:
			HandleMessage(ev.payload);
			break;

		default:
			break;
		}
	}
}

void ClientSession::HandleMessage(const std::vector<uint8_t> &packet)
{
	NetReader reader(packet);
	MsgType type = static_cast<MsgType>(reader.U8());
	switch (type)
	{
	case MsgType::Welcome:
	{
		WelcomeMsg msg;
		if (Decode(reader, msg))
		{
			_myIndex = msg.yourPlayerIndex;
			_lobby = std::move(msg.lobby);
			_state = State::InLobby;
			_status = "Waiting for the host to start the game";
			_logger.Printf(0, "net: joined lobby as player %u", _myIndex);
			if (_handler)
				_handler->OnLobbyChanged(_lobby);
		}
		break;
	}

	case MsgType::LobbyState:
	{
		LobbyStateMsg msg;
		if (Decode(reader, msg) && _state == State::InLobby)
		{
			_lobby = std::move(msg);
			if (_handler)
				_handler->OnLobbyChanged(_lobby);
		}
		break;
	}

	case MsgType::Reject:
	{
		RejectMsg msg;
		if (Decode(reader, msg))
		{
			_rejected = true;
			_rejectReason = msg.reason;
			_logger.Printf(1, "net: rejected by the host: %s", msg.text.c_str());
			SetDisconnected(msg.text);
		}
		break;
	}

	case MsgType::StartGame:
	{
		StartGameMsg msg;
		if (Decode(reader, msg) && _state == State::InLobby)
		{
			_lobby = msg.lobby;
			_started = true;
			_state = State::InGame;
			_status = "Game started";
			if (_handler)
				_handler->OnStartGame(msg);
		}
		break;
	}

	case MsgType::InputBatch:
	{
		InputBatchMsg msg;
		if (Decode(reader, msg) && _state == State::InGame && _handler)
			_handler->OnInputBatch(std::move(msg));
		break;
	}

	default:
		_logger.Printf(1, "net: unexpected message %u from the host", (unsigned)type);
		break;
	}
}

void ClientSession::SetDisconnected(std::string status)
{
	_state = State::Disconnected;
	_status = std::move(status);
	_logger.Printf(0, "net: %s", _status.c_str());
	if (_handler)
		_handler->OnDisconnected(_status);
}
