#pragma once
#include "NetProtocol.h"
#include <map>
#include <vector>

namespace Net
{
	// Host side: gathers every human player's input per tick.
	// Disconnected players are treated as always ready with a neutral input.
	class LockstepCollector final
	{
	public:
		void Reset(unsigned numHumans);

		void SetConnected(unsigned playerIndex, bool connected);
		bool IsConnected(unsigned playerIndex) const;

		// Ignores inputs for ticks that were already taken
		void AddInput(unsigned playerIndex, uint32_t tick, const VehicleState &state);
		bool IsTickComplete(uint32_t tick) const;
		int FirstMissingPlayer(uint32_t tick) const; // -1 if complete

		// Returns the inputs of all human players for the tick and forgets everything up to it
		std::vector<PlayerInput> TakeTick(uint32_t tick);

	private:
		struct Slot
		{
			bool connected = true;
			std::map<uint32_t, VehicleState> inputs;
		};
		std::vector<Slot> _slots;
		uint32_t _nextTick = 0;
	};

	// Client side: input batches received from the host, possibly out of order
	class BatchQueue final
	{
	public:
		void Add(InputBatchMsg &&batch);
		bool Has(uint32_t tick) const;
		InputBatchMsg Take(uint32_t tick); // forgets everything up to and including the tick
		size_t GetSize() const { return _batches.size(); }

	private:
		std::map<uint32_t, InputBatchMsg> _batches;
	};
}
