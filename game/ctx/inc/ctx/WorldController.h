#pragma once
#include <gc/detail/PtrList.h> // fixme: detail
#include <gc/VehicleState.h>
#include <map>
#include <set>
#include <vector>

class World;
class GC_Object;
class GC_Player;

class WorldController final
{
public:
	explicit WorldController(World &world);

	// Human players controlled from this machine
	std::vector<GC_Player*> GetLocalPlayers() const;
	// All human players, local and remote
	std::vector<GC_Player*> GetHumanPlayers() const;
	std::vector<GC_Player*> GetAIPlayers() const;

	void SetLocalPlayer(const GC_Player &player, bool isLocal);
	bool IsLocalPlayer(const GC_Player &player) const;

	typedef std::map<PtrList<GC_Object>::id_type, VehicleState> ControllerStateMap;
	void SendControllerStates(ControllerStateMap stateMap);

private:
	World &_world;
	std::set<PtrList<GC_Object>::id_type> _localPlayerIds; // not part of the world state
};
