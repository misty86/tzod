#include "inc/ctx/WorldController.h"
#include <gc/Player.h>
#include <gc/Vehicle.h>
#include <gc/World.h>
#include <gc/Macros.h>


WorldController::WorldController(World &world)
	: _world(world)
{
}

std::vector<GC_Player*> WorldController::GetLocalPlayers() const
{
	std::vector<GC_Player*> players;
	FOREACH(_world.GetList(LIST_players), GC_Player, player)
	{
		if (player->GetIsHuman() && IsLocalPlayer(*player))
		{
			players.push_back(player);
		}
	}
	return players;
}

std::vector<GC_Player*> WorldController::GetHumanPlayers() const
{
	std::vector<GC_Player*> players;
	FOREACH(_world.GetList(LIST_players), GC_Player, player)
	{
		if (player->GetIsHuman())
		{
			players.push_back(player);
		}
	}
	return players;
}

void WorldController::SetLocalPlayer(const GC_Player &player, bool isLocal)
{
	if (isLocal)
		_localPlayerIds.insert(player.GetId());
	else
		_localPlayerIds.erase(player.GetId());
}

bool WorldController::IsLocalPlayer(const GC_Player &player) const
{
	return _localPlayerIds.count(player.GetId()) != 0;
}

std::vector<GC_Player*> WorldController::GetAIPlayers() const
{
	std::vector<GC_Player*> players;
	FOREACH(_world.GetList(LIST_players), GC_Player, player)
	{
		if (!player->GetIsHuman())
		{
			players.push_back(player);
		}
	}
	return players;
}

void WorldController::SendControllerStates(ControllerStateMap stateMap)
{
	for (auto playerState: stateMap)
	{
		auto vehicle = static_cast<GC_Vehicle*>(_world.GetList(LIST_objects).at(playerState.first));
		vehicle->SetControllerState(playerState.second);
	}
}
