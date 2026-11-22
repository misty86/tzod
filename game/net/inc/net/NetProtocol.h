#pragma once
#include <gc/VehicleState.h>
#include <net/NetBuffer.h>
#include <cstdint>
#include <string>
#include <vector>

namespace FS
{
	struct Stream;
}

namespace Net
{
	// Bump on any change of the wire format or of anything that affects the simulation
	constexpr uint32_t NET_PROTOCOL_VERSION = 2;
	constexpr unsigned int MAX_NET_PLAYERS = 16; // humans + bots

	enum class MsgType : uint8_t
	{
		Invalid = 0,
		Hello,        // client -> host, right after the connection is established
		Welcome,      // host -> client, accepted into the lobby
		Reject,       // host -> client, followed by a disconnect
		LobbyState,   // host -> clients, whenever the lobby changes
		StartGame,    // host -> clients
		Input,        // client -> host, local input for one tick
		InputBatch,   // host -> clients, everybody's input for one tick
		Ready,        // client -> host, toggles the "I am ready" flag in the lobby
	};

	enum class RejectReason : uint8_t
	{
		VersionMismatch = 1,
		ServerFull,
		GameAlreadyStarted,
		BadMessage,
	};

	// Reason codes passed with a graceful disconnect
	enum class DisconnectReason : uint32_t
	{
		Timeout = 0, // ENet reports 0 when the peer timed out
		HostLeft,
		ClientLeft,
		Rejected,
	};

	struct NetPlayerDesc
	{
		std::string nick;
		std::string skin;
		std::string cls;
		uint8_t team = 0;
	};

	struct HelloMsg
	{
		uint32_t protocolVersion = NET_PROTOCOL_VERSION;
		NetPlayerDesc player;
	};

	struct LobbyStateMsg
	{
		std::string mapName;
		uint32_t mapHash = 0;
		std::vector<NetPlayerDesc> humans; // humans[0] is always the host; index == network player index
		std::vector<NetPlayerDesc> bots;   // network player index == humans.size() + bot index
		uint8_t difficulty = 0;
		int32_t fragLimit = 0;
		float timeLimit = 0;
		uint32_t readyMask = 1; // bit i set: human i pressed "ready"; the host (bit 0) is always ready

		bool IsReady(unsigned humanIndex) const { return humanIndex < 32 && 0 != (readyMask & (1u << humanIndex)); }
		bool IsEveryoneReady() const
		{
			for (unsigned i = 0; i < humans.size(); ++i)
				if (!IsReady(i))
					return false;
			return true;
		}
	};

	struct WelcomeMsg
	{
		uint8_t yourPlayerIndex = 0;
		LobbyStateMsg lobby;
	};

	struct RejectMsg
	{
		RejectReason reason = RejectReason::BadMessage;
		std::string text;
	};

	struct StartGameMsg
	{
		LobbyStateMsg lobby;
		uint32_t seed = 0;
		float tickRate = 60;
		uint8_t inputDelay = 3;
		bool debugChecksum = false;
	};

	struct InputMsg
	{
		uint32_t tick = 0;
		VehicleState state;
	};

	struct ReadyMsg
	{
		bool ready = false;
	};

	struct PlayerInput
	{
		uint8_t playerIndex = 0;
		VehicleState state;
	};

	struct InputBatchMsg
	{
		uint32_t tick = 0;
		std::vector<PlayerInput> inputs;
		uint32_t leftMask = 0;     // bit i set: player i disconnected before this tick
		bool hasChecksum = false;
		uint32_t checksum = 0;     // host world checksum before applying this tick
	};

	VehicleState NeutralVehicleState();

	// Each Encode() produces a complete packet starting with the MsgType byte.
	// Each Decode() expects the reader to be positioned right after the MsgType byte
	// and returns false if the packet is malformed.
	MsgType PeekMsgType(const std::vector<uint8_t> &packet);

	std::vector<uint8_t> Encode(const HelloMsg &msg);
	std::vector<uint8_t> Encode(const WelcomeMsg &msg);
	std::vector<uint8_t> Encode(const RejectMsg &msg);
	std::vector<uint8_t> Encode(const LobbyStateMsg &msg);
	std::vector<uint8_t> Encode(const StartGameMsg &msg);
	std::vector<uint8_t> Encode(const InputMsg &msg);
	std::vector<uint8_t> Encode(const InputBatchMsg &msg);
	std::vector<uint8_t> Encode(const ReadyMsg &msg);

	bool Decode(NetReader &reader, HelloMsg &msg);
	bool Decode(NetReader &reader, WelcomeMsg &msg);
	bool Decode(NetReader &reader, RejectMsg &msg);
	bool Decode(NetReader &reader, LobbyStateMsg &msg);
	bool Decode(NetReader &reader, StartGameMsg &msg);
	bool Decode(NetReader &reader, InputMsg &msg);
	bool Decode(NetReader &reader, InputBatchMsg &msg);
	bool Decode(NetReader &reader, ReadyMsg &msg);

	// FNV-1a
	uint32_t Fnv1a(const void *data, size_t size, uint32_t hash = 2166136261u);
	uint32_t HashStream(FS::Stream &stream);
}
