#pragma once
#include "Lockstep.h"
#include "NetSession.h"
#include <ctx/GameContext.h>
#include <cstdint>
#include <memory>
#include <string>

namespace Plat
{
	class ConsoleBuffer;
}

// Builds the game settings every peer must agree on from the lobby state
DMSettings MakeDMSettings(const Net::LobbyStateMsg &lobby);

// Deterministic lockstep game. The world is simulated with a fixed tick on every peer;
// the host gathers everybody's input for a tick and broadcasts it, nobody runs ahead.
class NetworkGameContext final
	: public GameContext
	, private Net::SessionHandler
{
public:
	NetworkGameContext(std::unique_ptr<World> world,
	                   const DMSettings &settings,
	                   uint32_t seed,
	                   std::shared_ptr<Net::NetSession> session,
	                   unsigned localPlayerIndex,
	                   float tickRate,
	                   unsigned inputDelay,
	                   bool debugChecksum,
	                   Plat::ConsoleBuffer &logger);
	~NetworkGameContext() override;

	bool IsConnectionLost() const { return _connectionLost; }
	bool IsStalled() const { return _stallTime > 0.5f; }
	uint32_t GetTick() const { return _tick; }
	unsigned GetLocalPlayerIndex() const { return _localIndex; }

	// GameContext
	bool IsGameplayActive() const override;
	bool IsGameOver() const override;
	void SetLocalControllerStates(WorldController::ControllerStateMap states) override;

	// GameContextBase
	void Step(float dt, AppConfig &appConfig, bool *outConfigChanged) override;

private:
	std::shared_ptr<Net::NetSession> _session;
	Net::HostSession *_hostSession = nullptr;     // one of these two is set
	Net::ClientSession *_clientSession = nullptr;
	Plat::ConsoleBuffer &_logger;

	const float _tickDt;
	const unsigned _delay;
	const unsigned _localIndex;
	const unsigned _numHumans;
	const unsigned _numBots;
	const bool _debugChecksum;

	uint32_t _tick = 0;                // next tick to simulate
	int64_t _lastSentInputTick = -1;
	float _accumulator = 0;
	VehicleState _pendingLocal;        // latest local input, sampled once per tick

	Net::LockstepCollector _collector; // host
	Net::BatchQueue _queue;            // client
	uint32_t _pendingLeftMask = 0;     // host

	bool _connectionLost = false;
	bool _desyncReported = false;
	float _stallTime = 0;
	bool _stallReported = false;

	void SendLocalInputAhead();
	bool TryTick();
	void ApplyBatch(const Net::InputBatchMsg &batch);
	void Announce(std::string text);
	std::string GetPlayerNick(unsigned playerIndex) const;

	// Net::SessionHandler
	void OnPlayerLeft(unsigned playerIndex, const std::string &nick) override;
	void OnInput(unsigned playerIndex, const Net::InputMsg &msg) override;
	void OnInputBatch(Net::InputBatchMsg &&batch) override;
	void OnDisconnected(const std::string &reason) override;
};
