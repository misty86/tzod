#include "inc/net/NetProtocol.h"
#include <fs/FileSystem.h>
#include <cstring>

using namespace Net;

VehicleState Net::NeutralVehicleState()
{
	VehicleState vs;
	std::memset(&vs, 0, sizeof(vs));
	return vs;
}

MsgType Net::PeekMsgType(const std::vector<uint8_t> &packet)
{
	return packet.empty() ? MsgType::Invalid : static_cast<MsgType>(packet[0]);
}

namespace
{
	void WriteVehicleState(NetWriter &w, const VehicleState &vs)
	{
		w.F32(vs.steering.x);
		w.F32(vs.steering.y);
		w.F32(vs.gas);
		w.F32(vs.weaponAngle);
		uint8_t bits = 0;
		if (vs.rotateWeapon) bits |= 1;
		if (vs.attack) bits |= 2;
		if (vs.pickup) bits |= 4;
		if (vs.light) bits |= 8;
		w.U8(bits);
	}

	VehicleState ReadVehicleState(NetReader &r)
	{
		VehicleState vs = NeutralVehicleState();
		vs.steering.x = r.F32();
		vs.steering.y = r.F32();
		vs.gas = r.F32();
		vs.weaponAngle = r.F32();
		uint8_t bits = r.U8();
		vs.rotateWeapon = !!(bits & 1);
		vs.attack = !!(bits & 2);
		vs.pickup = !!(bits & 4);
		vs.light = !!(bits & 8);
		return vs;
	}

	void WritePlayerDesc(NetWriter &w, const NetPlayerDesc &pd)
	{
		w.Str(pd.nick);
		w.Str(pd.skin);
		w.Str(pd.cls);
		w.U8(pd.team);
	}

	NetPlayerDesc ReadPlayerDesc(NetReader &r)
	{
		NetPlayerDesc pd;
		pd.nick = r.Str();
		pd.skin = r.Str();
		pd.cls = r.Str();
		pd.team = r.U8();
		return pd;
	}

	void WritePlayerList(NetWriter &w, const std::vector<NetPlayerDesc> &players)
	{
		w.U8(static_cast<uint8_t>(players.size()));
		for (auto &pd : players)
			WritePlayerDesc(w, pd);
	}

	bool ReadPlayerList(NetReader &r, std::vector<NetPlayerDesc> &players)
	{
		unsigned count = r.U8();
		if (count > MAX_NET_PLAYERS)
			return false;
		players.clear();
		players.reserve(count);
		for (unsigned i = 0; i < count; ++i)
			players.push_back(ReadPlayerDesc(r));
		return r.Ok();
	}

	void WriteLobby(NetWriter &w, const LobbyStateMsg &msg)
	{
		w.Str(msg.mapName);
		w.U32(msg.mapHash);
		WritePlayerList(w, msg.humans);
		WritePlayerList(w, msg.bots);
		w.U8(msg.difficulty);
		w.I32(msg.fragLimit);
		w.F32(msg.timeLimit);
		w.U32(msg.readyMask);
	}

	bool ReadLobby(NetReader &r, LobbyStateMsg &msg)
	{
		msg.mapName = r.Str();
		msg.mapHash = r.U32();
		if (!ReadPlayerList(r, msg.humans) || !ReadPlayerList(r, msg.bots))
			return false;
		msg.difficulty = r.U8();
		msg.fragLimit = r.I32();
		msg.timeLimit = r.F32();
		msg.readyMask = r.U32();
		return r.Ok() && !msg.humans.empty();
	}

	NetWriter Begin(MsgType type)
	{
		NetWriter w;
		w.U8(static_cast<uint8_t>(type));
		return w;
	}
}

///////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> Net::Encode(const HelloMsg &msg)
{
	NetWriter w = Begin(MsgType::Hello);
	w.U32(msg.protocolVersion);
	WritePlayerDesc(w, msg.player);
	return w.Take();
}

bool Net::Decode(NetReader &r, HelloMsg &msg)
{
	msg.protocolVersion = r.U32();
	msg.player = ReadPlayerDesc(r);
	return r.Ok();
}

std::vector<uint8_t> Net::Encode(const WelcomeMsg &msg)
{
	NetWriter w = Begin(MsgType::Welcome);
	w.U8(msg.yourPlayerIndex);
	WriteLobby(w, msg.lobby);
	return w.Take();
}

bool Net::Decode(NetReader &r, WelcomeMsg &msg)
{
	msg.yourPlayerIndex = r.U8();
	return ReadLobby(r, msg.lobby) && msg.yourPlayerIndex < msg.lobby.humans.size();
}

std::vector<uint8_t> Net::Encode(const RejectMsg &msg)
{
	NetWriter w = Begin(MsgType::Reject);
	w.U8(static_cast<uint8_t>(msg.reason));
	w.Str(msg.text);
	return w.Take();
}

bool Net::Decode(NetReader &r, RejectMsg &msg)
{
	msg.reason = static_cast<RejectReason>(r.U8());
	msg.text = r.Str();
	return r.Ok();
}

std::vector<uint8_t> Net::Encode(const LobbyStateMsg &msg)
{
	NetWriter w = Begin(MsgType::LobbyState);
	WriteLobby(w, msg);
	return w.Take();
}

bool Net::Decode(NetReader &r, LobbyStateMsg &msg)
{
	return ReadLobby(r, msg);
}

std::vector<uint8_t> Net::Encode(const StartGameMsg &msg)
{
	NetWriter w = Begin(MsgType::StartGame);
	WriteLobby(w, msg.lobby);
	w.U32(msg.seed);
	w.F32(msg.tickRate);
	w.U8(msg.inputDelay);
	w.Bool(msg.debugChecksum);
	return w.Take();
}

bool Net::Decode(NetReader &r, StartGameMsg &msg)
{
	if (!ReadLobby(r, msg.lobby))
		return false;
	msg.seed = r.U32();
	msg.tickRate = r.F32();
	msg.inputDelay = r.U8();
	msg.debugChecksum = r.Bool();
	return r.Ok() && msg.tickRate > 0 && msg.tickRate <= 1000;
}

std::vector<uint8_t> Net::Encode(const InputMsg &msg)
{
	NetWriter w = Begin(MsgType::Input);
	w.U32(msg.tick);
	WriteVehicleState(w, msg.state);
	return w.Take();
}

bool Net::Decode(NetReader &r, InputMsg &msg)
{
	msg.tick = r.U32();
	msg.state = ReadVehicleState(r);
	return r.Ok();
}

std::vector<uint8_t> Net::Encode(const InputBatchMsg &msg)
{
	NetWriter w = Begin(MsgType::InputBatch);
	w.U32(msg.tick);
	w.U8(static_cast<uint8_t>(msg.inputs.size()));
	for (auto &input : msg.inputs)
	{
		w.U8(input.playerIndex);
		WriteVehicleState(w, input.state);
	}
	w.U32(msg.leftMask);
	w.Bool(msg.hasChecksum);
	if (msg.hasChecksum)
		w.U32(msg.checksum);
	return w.Take();
}

bool Net::Decode(NetReader &r, InputBatchMsg &msg)
{
	msg.tick = r.U32();
	unsigned count = r.U8();
	if (count > MAX_NET_PLAYERS)
		return false;
	msg.inputs.clear();
	msg.inputs.reserve(count);
	for (unsigned i = 0; i < count; ++i)
	{
		PlayerInput input;
		input.playerIndex = r.U8();
		input.state = ReadVehicleState(r);
		msg.inputs.push_back(input);
	}
	msg.leftMask = r.U32();
	msg.hasChecksum = r.Bool();
	msg.checksum = msg.hasChecksum ? r.U32() : 0;
	return r.Ok();
}

std::vector<uint8_t> Net::Encode(const ReadyMsg &msg)
{
	NetWriter w = Begin(MsgType::Ready);
	w.Bool(msg.ready);
	return w.Take();
}

bool Net::Decode(NetReader &r, ReadyMsg &msg)
{
	msg.ready = r.Bool();
	return r.Ok();
}

///////////////////////////////////////////////////////////////////////////////

uint32_t Net::Fnv1a(const void *data, size_t size, uint32_t hash)
{
	auto bytes = static_cast<const uint8_t*>(data);
	for (size_t i = 0; i < size; ++i)
	{
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

uint32_t Net::HashStream(FS::Stream &stream)
{
	uint32_t hash = 2166136261u;
	char buf[4096];
	for (;;)
	{
		size_t count = stream.Read(buf, 1, sizeof(buf));
		if (0 == count)
			break;
		hash = Fnv1a(buf, count, hash);
	}
	return hash;
}
