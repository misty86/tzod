#pragma once
#include "ScriptMessageBroadcaster.h"
#include "GameContextBase.h"
#include "GameEvents.h"
#include "WorldController.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class World;
class GC_Player;
struct Gameplay;

namespace FS
{
	struct Stream;
}

class AIManager;
class ScriptHarness;
class ThemeManager;
class TextureManager;

struct PlayerDesc
{
	std::string nick;
	std::string skin;
	std::string cls;
	unsigned int team;
};

enum class AIDiffuculty;

struct DMSettings
{
	std::vector<PlayerDesc> players;
	std::vector<PlayerDesc> bots;
	AIDiffuculty difficulty{};
	int fragLimit{};
	float timeLimit{};
};


class GameContext : public GameContextBase
{
public:
	GameContext(std::unique_ptr<World> world, const DMSettings &settings);
	GameContext(std::unique_ptr<World> world, const DMSettings &settings, unsigned long seed);
	virtual ~GameContext();
	WorldController& GetWorldController() { return *_worldController; }
	const WorldController& GetWorldController() const { return *_worldController; }
	GameEventSource& GetGameEventSource() { return _gameEventsBroadcaster; }
	ScriptMessageSource& GetScriptMessageSource() { return _scriptMessageBroadcaster; }
	AIManager& GetAIManager() { return *_aiManager; }
	AIDiffuculty GetDifficulty() const { return _difficulty; }
	float GetGameplayTime() const { return _gameplayTime; }
	virtual bool IsGameplayActive() const;
	virtual bool IsGameOver() const;

	// Players created from DMSettings in order: humans first, then bots. nullptr if out of range.
	GC_Player* GetDMPlayer(size_t index) const;
	size_t GetDMPlayerCount() const { return _dmPlayerIds.size(); }

	// Controller states of the local human players collected by the UI.
	// The default implementation applies them to the world immediately.
	virtual void SetLocalControllerStates(WorldController::ControllerStateMap states);

	void Serialize(FS::Stream &stream);
	void Deserialize(FS::Stream &stream);

	// GameContextBase
	World& GetWorld() override { return *_world; }
	Gameplay* GetGameplay() const override;
	void Step(float dt, AppConfig &appConfig, bool *outConfigChanged) override;
	bool IsWorldActive() const override;

protected:
	// Advances the world and the scripts by dt without touching the AI or the gameplay clock
	void StepSimulation(float dt);
	void AddGameplayTime(float dt) { _gameplayTime += dt; }
	GameEventsBroadcaster& GetGameEventsBroadcaster() { return _gameEventsBroadcaster; }

private:
	GameEventsBroadcaster _gameEventsBroadcaster;
	app_detail::ScriptMessageBroadcaster _scriptMessageBroadcaster;
	std::unique_ptr<World> _world;
	std::unique_ptr<WorldController> _worldController;
	std::unique_ptr<Gameplay> _gameplay;
	std::unique_ptr<ScriptHarness> _scriptHarness;
	std::unique_ptr<AIManager> _aiManager;
	std::vector<PtrList<GC_Object>::id_type> _dmPlayerIds;
	const AIDiffuculty _difficulty;
	float _gameplayTime = 0;
};

class GameContextCampaignDM final
	: public GameContext
{
public:
	GameContextCampaignDM(std::unique_ptr<World> world, const DMSettings &settings, int campaignTier, int campaignMap);

	int GetRating() const;

	// GameContextBase
	void Step(float dt, AppConfig &appConfig, bool *outConfigChanged) override;

private:
	bool IsVictory() const;
	const int _campaignTier;
	const int _campaignMap;
};
