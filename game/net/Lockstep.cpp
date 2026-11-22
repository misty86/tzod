#include "inc/net/Lockstep.h"
#include <cassert>

using namespace Net;

void LockstepCollector::Reset(unsigned numHumans)
{
	_slots.assign(numHumans, Slot());
	_nextTick = 0;
}

void LockstepCollector::SetConnected(unsigned playerIndex, bool connected)
{
	if (playerIndex < _slots.size())
		_slots[playerIndex].connected = connected;
}

bool LockstepCollector::IsConnected(unsigned playerIndex) const
{
	return playerIndex < _slots.size() && _slots[playerIndex].connected;
}

void LockstepCollector::AddInput(unsigned playerIndex, uint32_t tick, const VehicleState &state)
{
	if (playerIndex < _slots.size() && tick >= _nextTick)
		_slots[playerIndex].inputs[tick] = state;
}

bool LockstepCollector::IsTickComplete(uint32_t tick) const
{
	return -1 == FirstMissingPlayer(tick);
}

int LockstepCollector::FirstMissingPlayer(uint32_t tick) const
{
	for (size_t i = 0; i < _slots.size(); ++i)
	{
		const Slot &slot = _slots[i];
		if (slot.connected && 0 == slot.inputs.count(tick))
			return static_cast<int>(i);
	}
	return -1;
}

std::vector<PlayerInput> LockstepCollector::TakeTick(uint32_t tick)
{
	assert(IsTickComplete(tick));
	std::vector<PlayerInput> result;
	result.reserve(_slots.size());
	for (size_t i = 0; i < _slots.size(); ++i)
	{
		Slot &slot = _slots[i];
		PlayerInput input;
		input.playerIndex = static_cast<uint8_t>(i);
		auto it = slot.inputs.find(tick);
		input.state = (slot.inputs.end() != it) ? it->second : NeutralVehicleState();
		result.push_back(input);
		// forget this tick and everything older
		slot.inputs.erase(slot.inputs.begin(), slot.inputs.upper_bound(tick));
	}
	_nextTick = tick + 1;
	return result;
}

///////////////////////////////////////////////////////////////////////////////

void BatchQueue::Add(InputBatchMsg &&batch)
{
	_batches[batch.tick] = std::move(batch);
}

bool BatchQueue::Has(uint32_t tick) const
{
	return 0 != _batches.count(tick);
}

InputBatchMsg BatchQueue::Take(uint32_t tick)
{
	auto it = _batches.find(tick);
	assert(_batches.end() != it);
	InputBatchMsg result = std::move(it->second);
	_batches.erase(_batches.begin(), _batches.upper_bound(tick));
	return result;
}
