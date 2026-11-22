#include "inc/net/NetworkGameContext.h"
#include "inc/net/WorldChecksum.h"
#include <ai/ai.h>
#include <ctx/AIManager.h>
#include <ctx/Gameplay.h>
#include <ctx/WorldController.h>
#include <gc/Player.h>
#include <gc/Vehicle.h>
#include <gc/World.h>
#include <plat/ConsoleBuffer.h>
#include <algorithm>
#include <cassert>

namespace
{
	constexpr float c_maxAccumulatedTime = 0.25f; // seconds of catch-up we are willing to do
	constexpr int c_maxTicksPerFrame = 8;
	constexpr float c_stallReportDelay = 1.0f;

	PlayerDesc ToPlayerDesc(const Net::NetPlayerDesc &pd)
	{
		PlayerDesc result;
		result.nick = pd.nick;
		result.skin = pd.skin;
		result.cls = pd.cls;
		result.team = pd.team;
		return result;
	}
}

DMSettings MakeDMSettings(const Net::LobbyStateMsg &lobby)
{
	DMSettings settings;
	for (auto &pd : lobby.humans)
		settings.players.push_back(ToPlayerDesc(pd));
	for (auto &pd : lobby.bots)
		settings.bots.push_back(ToPlayerDesc(pd));
	settings.difficulty = static_cast<AIDiffuculty>(std::clamp<int>(lobby.difficulty, 0, 2));
	settings.fragLimit = lobby.fragLimit;
	settings.timeLimit = lobby.timeLimit;
	return settings;
}

///////////////////////////////////////////////////////////////////////////////

NetworkGameContext::NetworkGameContext(std::unique_ptr<World> world,
                                       const DMSettings &settings,
                                       uint32_t seed,
                                       std::shared_ptr<Net::NetSession> session,
                                       unsigned localPlayerIndex,
                                       float tickRate,
                                       unsigned inputDelay,
                                       bool debugChecksum,
                                       Plat::ConsoleBuffer &logger)
	: GameContext(std::move(world), settings, seed)
	, _session(std::move(session))
	, _logger(logger)
	, _tickDt(1.0f / std::max(1.0f, tickRate))
	, _delay(inputDelay)
	, _localIndex(localPlayerIndex)
	, _numHumans(static_cast<unsigned>(settings.players.size()))
	, _numBots(static_cast<unsigned>(settings.bots.size()))
	, _debugChecksum(debugChecksum)
	, _pendingLocal(Net::NeutralVehicleState())
{
	assert(_session);
	assert(_localIndex < _numHumans);

	if (_session->IsHost())
		_hostSession = static_cast<Net::HostSession*>(_session.get());
	else
		_clientSession = static_cast<Net::ClientSession*>(_session.get());

	// only the player at the local index is controlled from this machine
	for (auto player : GetWorldController().GetHumanPlayers())
		GetWorldController().SetLocalPlayer(*player, false);
	if (auto localPlayer = GetDMPlayer(_localIndex))
		GetWorldController().SetLocalPlayer(*localPlayer, true);

	if (_hostSession)
	{
		_collector.Reset(_numHumans);
		auto connected = _hostSession->GetConnectedPlayerIndices();
		for (unsigned i = 0; i < _numHumans; ++i)
		{
			if (connected.end() == std::find(connected.begin(), connected.end(), i))
				_collector.SetConnected(i, false);
		}
	}

	_session->SetHandler(this);
	_logger.Printf(0, "net: game context created, %s, local player %u, tick %.4f s, input delay %u ticks, checksum %s",
		_hostSession ? "host" : "client", _localIndex, _tickDt, _delay, _debugChecksum ? "on" : "off");
}

NetworkGameContext::~NetworkGameContext()
{
	if (_session->GetHandler() == this)
		_session->SetHandler(nullptr);
}

bool NetworkGameContext::IsGameplayActive() const
{
	// a local pause must not stop the shared world
	return !_connectionLost;
}

bool NetworkGameContext::IsGameOver() const
{
	return _connectionLost || GameContext::IsGameOver();
}

void NetworkGameContext::SetLocalControllerStates(WorldController::ControllerStateMap states)
{
	// the UI passes the states of the local players keyed by vehicle; there is exactly one local player
	_pendingLocal = states.empty() ? Net::NeutralVehicleState() : states.begin()->second;
}

void NetworkGameContext::Step(float dt, AppConfig &appConfig, bool *outConfigChanged)
{
	_session->Pump();
	if (_connectionLost)
		return;

	SendLocalInputAhead();

	_accumulator = std::min(_accumulator + dt, c_maxAccumulatedTime);

	bool progressed = false;
	for (int n = 0; n < c_maxTicksPerFrame && _accumulator >= _tickDt && IsWorldActive(); ++n)
	{
		if (!TryTick())
			break;
		_accumulator -= _tickDt;
		progressed = true;
		SendLocalInputAhead();
	}

	if (progressed || !IsWorldActive() || _accumulator < _tickDt)
	{
		_stallTime = 0;
		_stallReported = false;
	}
	else
	{
		_stallTime += dt;
		if (_stallTime > c_stallReportDelay && !_stallReported)
		{
			_stallReported = true;
			if (_hostSession)
			{
				int missing = _collector.FirstMissingPlayer(_tick);
				Announce("Waiting for " + (missing >= 0 ? GetPlayerNick(static_cast<unsigned>(missing)) : std::string("players")) + "...");
			}
			else
			{
				Announce("Waiting for the host...");
			}
		}
	}
}

void NetworkGameContext::SendLocalInputAhead()
{
	while (_lastSentInputTick < static_cast<int64_t>(_tick) + static_cast<int64_t>(_delay))
	{
		uint32_t tick = static_cast<uint32_t>(++_lastSentInputTick);
		if (_hostSession)
		{
			_collector.AddInput(_localIndex, tick, _pendingLocal);
		}
		else
		{
			Net::InputMsg msg;
			msg.tick = tick;
			msg.state = _pendingLocal;
			_clientSession->SendInput(msg);
		}
	}
}

bool NetworkGameContext::TryTick()
{
	Net::InputBatchMsg batch;

	if (_hostSession)
	{
		if (!_collector.IsTickComplete(_tick))
			return false;

		batch.tick = _tick;
		batch.inputs = _collector.TakeTick(_tick);

		// the bots are driven by the host only; their input travels with everybody else's
		if (_numBots > 0)
		{
			auto aiStates = GetAIManager().ComputeAIState(GetWorld(), _tickDt);
			for (unsigned bot = 0; bot < _numBots; ++bot)
			{
				unsigned playerIndex = _numHumans + bot;
				if (auto player = GetDMPlayer(playerIndex))
				{
					if (auto vehicle = player->GetVehicle())
					{
						auto it = aiStates.find(vehicle->GetId());
						if (aiStates.end() != it)
						{
							Net::PlayerInput input;
							input.playerIndex = static_cast<uint8_t>(playerIndex);
							input.state = it->second;
							batch.inputs.push_back(input);
						}
					}
				}
			}
		}

		batch.leftMask = _pendingLeftMask;
		_pendingLeftMask = 0;

		if (_debugChecksum)
		{
			batch.hasChecksum = true;
			batch.checksum = Net::ComputeWorldChecksum(GetWorld());
		}

		_hostSession->BroadcastBatch(batch);
	}
	else
	{
		if (!_queue.Has(_tick))
			return false;

		batch = _queue.Take(_tick);

		if (batch.hasChecksum && !_desyncReported)
		{
			uint32_t local = Net::ComputeWorldChecksum(GetWorld());
			if (local != batch.checksum)
			{
				_desyncReported = true;
				_logger.Printf(1, "net: DESYNC at tick %u: local checksum 0x%08x, host checksum 0x%08x", _tick, local, batch.checksum);
				Announce("DESYNC detected, see the log");
			}
		}
	}

	ApplyBatch(batch);
	StepSimulation(_tickDt);
	AddGameplayTime(_tickDt);
	++_tick;
	if (0 == _tick % 600)
		_logger.Printf(0, "net: tick %u, world time %.1f s", _tick, GetWorld().GetTime());
	return true;
}

void NetworkGameContext::ApplyBatch(const Net::InputBatchMsg &batch)
{
	WorldController::ControllerStateMap states;
	for (auto &input : batch.inputs)
	{
		if (auto player = GetDMPlayer(input.playerIndex))
		{
			if (auto vehicle = player->GetVehicle())
				states[vehicle->GetId()] = input.state;
		}
	}
	GetWorldController().SendControllerStates(std::move(states));

	for (unsigned i = 0; i < 32 && i < _numHumans; ++i)
	{
		if (batch.leftMask & (1u << i))
			Announce(GetPlayerNick(i) + " left the game");
	}
}

void NetworkGameContext::Announce(std::string text)
{
	_logger.Printf(0, "net: %s", text.c_str());
	GetGameEventsBroadcaster().OnGameMessage(text);
}

std::string NetworkGameContext::GetPlayerNick(unsigned playerIndex) const
{
	if (auto player = GetDMPlayer(playerIndex))
		return std::string(player->GetNick());
	return "player " + std::to_string(playerIndex);
}

void NetworkGameContext::OnPlayerLeft(unsigned playerIndex, const std::string &nick)
{
	if (_hostSession)
	{
		// keep the game going; the slot is complete from now on and the tank idles
		_collector.SetConnected(playerIndex, false);
		if (playerIndex < 32)
			_pendingLeftMask |= (1u << playerIndex);
	}
}

void NetworkGameContext::OnInput(unsigned playerIndex, const Net::InputMsg &msg)
{
	assert(_hostSession);
	_collector.AddInput(playerIndex, msg.tick, msg.state);
}

void NetworkGameContext::OnInputBatch(Net::InputBatchMsg &&batch)
{
	assert(_clientSession);
	if (batch.tick >= _tick)
		_queue.Add(std::move(batch));
}

void NetworkGameContext::OnDisconnected(const std::string &reason)
{
	assert(_clientSession);
	if (!_connectionLost)
	{
		_connectionLost = true;
		Announce(reason);
	}
}
