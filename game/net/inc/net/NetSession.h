#pragma once
#include "NetProtocol.h"
#include <net/NetHost.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Plat
{
	class ConsoleBuffer;
}

namespace Net
{
	// Receives decoded messages from a session. Set by the lobby UI first, then by the game context.
	struct SessionHandler
	{
		virtual void OnLobbyChanged(const LobbyStateMsg &lobby) {}
		virtual void OnPlayerJoined(unsigned playerIndex, const std::string &nick) {}
		// In the lobby the player is removed and the indices above it shift down.
		// After the game has started the slot stays and is just marked as gone.
		virtual void OnPlayerLeft(unsigned playerIndex, const std::string &nick) {}
		virtual void OnStartGame(const StartGameMsg &msg) {}       // client only
		virtual void OnInput(unsigned playerIndex, const InputMsg &msg) {} // host only
		virtual void OnInputBatch(InputBatchMsg &&batch) {}        // client only
		virtual void OnDisconnected(const std::string &reason) {}  // client only: lost the host or was rejected
	};

	class NetSession
	{
	public:
		virtual ~NetSession() = default;

		// Processes incoming packets and dispatches them to the handler. Call once per frame.
		virtual void Pump() = 0;
		virtual bool IsHost() const = 0;

		void SetHandler(SessionHandler *handler) { _handler = handler; }
		SessionHandler* GetHandler() const { return _handler; }

		const LobbyStateMsg& GetLobby() const { return _lobby; }
		bool IsGameStarted() const { return _started; }

	protected:
		SessionHandler *_handler = nullptr;
		LobbyStateMsg _lobby;
		bool _started = false;
	};

	class HostSession final : public NetSession
	{
	public:
		// Throws std::runtime_error if the port could not be bound
		HostSession(uint16_t port, NetPlayerDesc hostPlayer, Plat::ConsoleBuffer &logger);
		~HostSession() override; // disconnects all clients gracefully

		void SetMap(std::string mapName, uint32_t mapHash);
		void AddBot(NetPlayerDesc bot);
		void RemoveBot(unsigned botIndex);
		void SetRules(uint8_t difficulty, int fragLimit, float timeLimit);

		// Freezes the lobby and tells everybody to start. New connections are rejected afterwards.
		StartGameMsg StartGame(uint32_t seed, float tickRate, uint8_t inputDelay, bool debugChecksum);
		void BroadcastBatch(const InputBatchMsg &batch);

		std::vector<unsigned> GetConnectedPlayerIndices() const; // host (0) included
		uint32_t GetMaxRoundTripTime() const;
		uint16_t GetPort() const;

		// NetSession
		void Pump() override;
		bool IsHost() const override { return true; }

	private:
		struct Client
		{
			PeerHandle peer;
			int playerIndex; // -1 until the Hello message arrives
		};

		std::unique_ptr<NetHost> _host;
		std::vector<Client> _clients;
		Plat::ConsoleBuffer &_logger;

		int FindClient(PeerHandle peer) const;
		void HandleMessage(size_t clientIndex, const std::vector<uint8_t> &packet);
		void RemoveClient(size_t clientIndex);
		void Reject(size_t clientIndex, RejectReason reason, std::string text);
		void BroadcastLobby();
		void SendToPlayers(const std::vector<uint8_t> &packet);
	};

	class ClientSession final : public NetSession
	{
	public:
		enum class State
		{
			Connecting,
			Handshaking,
			InLobby,
			InGame,
			Disconnected,
		};

		// Throws std::runtime_error if the host name could not be resolved
		ClientSession(std::string_view hostname, uint16_t port, NetPlayerDesc me, Plat::ConsoleBuffer &logger);
		~ClientSession() override; // leaves gracefully

		State GetState() const { return _state; }
		// Valid when the state is Disconnected: the reason the host gave, or Timeout
		bool WasRejected() const { return _rejected; }
		RejectReason GetRejectReason() const { return _rejectReason; }
		DisconnectReason GetDisconnectReason() const { return _disconnectReason; }
		const std::string& GetStatusText() const { return _status; }

		unsigned GetMyPlayerIndex() const { return _myIndex; }
		void SendInput(const InputMsg &msg);

		// Lobby "I am ready" flag; the host starts the game once everybody is ready
		void SetReady(bool ready);
		bool IsReady() const { return _lobby.IsReady(_myIndex); }
		uint32_t GetRoundTripTime() const;

		// NetSession
		void Pump() override;
		bool IsHost() const override { return false; }

	private:
		std::unique_ptr<NetHost> _host;
		PeerHandle _server = InvalidPeer;
		NetPlayerDesc _me;
		Plat::ConsoleBuffer &_logger;
		State _state = State::Connecting;
		std::string _status;
		unsigned _myIndex = 0;
		bool _rejected = false;
		RejectReason _rejectReason = RejectReason::BadMessage;
		DisconnectReason _disconnectReason = DisconnectReason::Timeout;

		void HandleMessage(const std::vector<uint8_t> &packet);
		void SetDisconnected(std::string status);
	};
}
