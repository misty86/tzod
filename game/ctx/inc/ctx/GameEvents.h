#pragma once
#include <string_view>
#include <unordered_set>

class GC_Player;

enum class MurderType
{
	Accident,
	Enemy,
	Friend,
	Suicide,
};

struct GameListener
{
	virtual void OnMurder(GC_Player &victim, GC_Player *killer, MurderType murderType) = 0;
	// Free-form notification for the player (e.g. "X joined", "connection lost")
	virtual void OnGameMessage(std::string_view text) {}
};

struct GameEventSource
{
	virtual void AddListener(GameListener &ls) = 0;
	virtual void RemoveListener(GameListener &ls) = 0;
};

class GameEventsBroadcaster final
	: public GameEventSource
	, public GameListener
{
public:
	~GameEventsBroadcaster();

	// GameEventSource
	void AddListener(GameListener &ls) override;
	void RemoveListener(GameListener &ls) override;

	// GameListener
	void OnMurder(GC_Player &victim, GC_Player *killer, MurderType murderType) override;
	void OnGameMessage(std::string_view text) override;

private:
	std::unordered_set<GameListener*> _listeners;
};
