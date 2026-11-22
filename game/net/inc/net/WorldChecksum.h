#pragma once
#include <cstdint>

class World;

namespace Net
{
	// Cheap per-tick fingerprint of the simulation state used to detect desync between peers.
	// Covers the world clock, the RNG seed, every vehicle and every player's score.
	uint32_t ComputeWorldChecksum(const World &world);
}
