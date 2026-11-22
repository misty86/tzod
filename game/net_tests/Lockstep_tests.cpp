#include <net/Lockstep.h>
#include <net/NetProtocol.h>
#include <gtest/gtest.h>

namespace
{
	VehicleState MakeState(float gas)
	{
		VehicleState vs = Net::NeutralVehicleState();
		vs.gas = gas;
		return vs;
	}
}

TEST(LockstepCollector, TickCompletesWhenEveryConnectedPlayerSentInput)
{
	Net::LockstepCollector collector;
	collector.Reset(3);

	EXPECT_FALSE(collector.IsTickComplete(0));
	EXPECT_EQ(0, collector.FirstMissingPlayer(0));

	collector.AddInput(0, 0, MakeState(1));
	collector.AddInput(2, 0, MakeState(3));
	EXPECT_FALSE(collector.IsTickComplete(0));
	EXPECT_EQ(1, collector.FirstMissingPlayer(0));

	collector.AddInput(1, 0, MakeState(2));
	EXPECT_TRUE(collector.IsTickComplete(0));
	EXPECT_EQ(-1, collector.FirstMissingPlayer(0));

	auto inputs = collector.TakeTick(0);
	ASSERT_EQ(3u, inputs.size());
	EXPECT_EQ(0, inputs[0].playerIndex);
	EXPECT_EQ(1.f, inputs[0].state.gas);
	EXPECT_EQ(2, inputs[2].playerIndex);
	EXPECT_EQ(3.f, inputs[2].state.gas);

	// the tick is gone now
	EXPECT_FALSE(collector.IsTickComplete(0));
}

TEST(LockstepCollector, DisconnectedPlayerIsAlwaysReadyWithNeutralInput)
{
	Net::LockstepCollector collector;
	collector.Reset(2);
	collector.AddInput(0, 5, MakeState(1));
	EXPECT_FALSE(collector.IsTickComplete(5));

	collector.SetConnected(1, false);
	EXPECT_TRUE(collector.IsTickComplete(5));

	auto inputs = collector.TakeTick(5);
	ASSERT_EQ(2u, inputs.size());
	EXPECT_EQ(0.f, inputs[1].state.gas);
	EXPECT_FALSE(inputs[1].state.attack);
}

TEST(LockstepCollector, LateInputForTakenTickIsIgnored)
{
	Net::LockstepCollector collector;
	collector.Reset(1);
	collector.AddInput(0, 0, MakeState(1));
	collector.AddInput(0, 1, MakeState(2));
	collector.TakeTick(1); // also drops tick 0

	collector.AddInput(0, 0, MakeState(9));
	EXPECT_FALSE(collector.IsTickComplete(0));
	collector.AddInput(0, 2, MakeState(3));
	EXPECT_TRUE(collector.IsTickComplete(2));
	EXPECT_EQ(3.f, collector.TakeTick(2)[0].state.gas);
}

TEST(BatchQueue, OutOfOrderBatches)
{
	Net::BatchQueue queue;
	Net::InputBatchMsg b1;
	b1.tick = 1;
	Net::InputBatchMsg b0;
	b0.tick = 0;
	b0.checksum = 42;
	queue.Add(std::move(b1));
	queue.Add(std::move(b0));

	EXPECT_TRUE(queue.Has(0));
	EXPECT_TRUE(queue.Has(1));
	EXPECT_FALSE(queue.Has(2));
	EXPECT_EQ(42u, queue.Take(0).checksum);
	EXPECT_FALSE(queue.Has(0));
	EXPECT_EQ(1u, queue.GetSize());
}

TEST(Protocol, InputBatchRoundTrip)
{
	Net::InputBatchMsg msg;
	msg.tick = 12345;
	msg.leftMask = 0x5;
	msg.hasChecksum = true;
	msg.checksum = 0xdeadbeef;
	Net::PlayerInput input;
	input.playerIndex = 7;
	input.state = Net::NeutralVehicleState();
	input.state.steering = vec2d{ 0.5f, -0.25f };
	input.state.gas = -1;
	input.state.weaponAngle = 1.5f;
	input.state.attack = true;
	input.state.light = true;
	msg.inputs.push_back(input);

	auto packet = Net::Encode(msg);
	ASSERT_EQ(Net::MsgType::InputBatch, Net::PeekMsgType(packet));

	Net::NetReader reader(packet);
	reader.U8(); // type
	Net::InputBatchMsg decoded;
	ASSERT_TRUE(Net::Decode(reader, decoded));
	EXPECT_EQ(msg.tick, decoded.tick);
	EXPECT_EQ(msg.leftMask, decoded.leftMask);
	EXPECT_TRUE(decoded.hasChecksum);
	EXPECT_EQ(msg.checksum, decoded.checksum);
	ASSERT_EQ(1u, decoded.inputs.size());
	EXPECT_EQ(7, decoded.inputs[0].playerIndex);
	EXPECT_EQ(0.5f, decoded.inputs[0].state.steering.x);
	EXPECT_EQ(-0.25f, decoded.inputs[0].state.steering.y);
	EXPECT_EQ(-1.f, decoded.inputs[0].state.gas);
	EXPECT_EQ(1.5f, decoded.inputs[0].state.weaponAngle);
	EXPECT_TRUE(decoded.inputs[0].state.attack);
	EXPECT_FALSE(decoded.inputs[0].state.pickup);
	EXPECT_TRUE(decoded.inputs[0].state.light);
	EXPECT_FALSE(decoded.inputs[0].state.rotateWeapon);
}

TEST(Protocol, StartGameRoundTrip)
{
	Net::StartGameMsg msg;
	msg.lobby.mapName = "dm1";
	msg.lobby.mapHash = 123;
	msg.lobby.humans.push_back({ "host", "red", "default", 1 });
	msg.lobby.humans.push_back({ "guest", "blue", "default", 2 });
	msg.lobby.bots.push_back({ "bot", "green", "default", 0 });
	msg.lobby.difficulty = 2;
	msg.lobby.fragLimit = 21;
	msg.lobby.timeLimit = 420;
	msg.seed = 0xabcdef;
	msg.tickRate = 60;
	msg.inputDelay = 3;
	msg.debugChecksum = true;

	auto packet = Net::Encode(msg);
	Net::NetReader reader(packet);
	ASSERT_EQ(Net::MsgType::StartGame, static_cast<Net::MsgType>(reader.U8()));
	Net::StartGameMsg decoded;
	ASSERT_TRUE(Net::Decode(reader, decoded));
	EXPECT_EQ("dm1", decoded.lobby.mapName);
	EXPECT_EQ(123u, decoded.lobby.mapHash);
	ASSERT_EQ(2u, decoded.lobby.humans.size());
	EXPECT_EQ("guest", decoded.lobby.humans[1].nick);
	EXPECT_EQ(2, decoded.lobby.humans[1].team);
	ASSERT_EQ(1u, decoded.lobby.bots.size());
	EXPECT_EQ(2, decoded.lobby.difficulty);
	EXPECT_EQ(21, decoded.lobby.fragLimit);
	EXPECT_EQ(420.f, decoded.lobby.timeLimit);
	EXPECT_EQ(0xabcdefu, decoded.seed);
	EXPECT_EQ(60.f, decoded.tickRate);
	EXPECT_EQ(3, decoded.inputDelay);
	EXPECT_TRUE(decoded.debugChecksum);
	EXPECT_TRUE(decoded.lobby.IsReady(0));
	EXPECT_FALSE(decoded.lobby.IsReady(1));
	EXPECT_FALSE(decoded.lobby.IsEveryoneReady());
}

TEST(Protocol, TruncatedPacketIsRejected)
{
	Net::StartGameMsg msg;
	msg.lobby.mapName = "dm1";
	msg.lobby.humans.push_back({ "host", "red", "default", 0 });
	auto packet = Net::Encode(msg);
	packet.resize(packet.size() / 2);

	Net::NetReader reader(packet);
	reader.U8();
	Net::StartGameMsg decoded;
	EXPECT_FALSE(Net::Decode(reader, decoded));
}
