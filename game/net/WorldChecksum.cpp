#include "inc/net/WorldChecksum.h"
#include "inc/net/NetProtocol.h"
#include <gc/Macros.h>
#include <gc/Player.h>
#include <gc/Vehicle.h>
#include <gc/World.h>

namespace
{
	struct Hasher
	{
		uint32_t hash = 2166136261u;

		template <class T>
		void Add(const T &value)
		{
			hash = Net::Fnv1a(&value, sizeof(value), hash);
		}
	};
}

uint32_t Net::ComputeWorldChecksum(const World &world)
{
	Hasher h;
	h.Add(world.GetTime());
	h.Add(static_cast<uint32_t>(world.GetSeed()));

	uint32_t vehicleCount = 0;
	FOREACH(world.GetList(LIST_vehicles), GC_Vehicle, vehicle)
	{
		++vehicleCount;
		vec2d pos = vehicle->GetPos();
		vec2d dir = vehicle->GetDirection();
		h.Add(pos.x);
		h.Add(pos.y);
		h.Add(dir.x);
		h.Add(dir.y);
		h.Add(vehicle->GetHealth());
	}
	h.Add(vehicleCount);

	FOREACH(world.GetList(LIST_players), GC_Player, player)
	{
		h.Add(player->GetScore());
		h.Add(player->GetNumDeaths());
	}

	return h.hash;
}
