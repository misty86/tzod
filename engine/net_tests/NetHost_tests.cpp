#include <net/NetHost.h>
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace
{
	// Pumps both hosts until the predicate holds or the timeout expires.
	template <class Pred>
	bool PumpUntil(Net::NetHost &a, Net::NetHost &b, std::vector<Net::NetEvent> &eventsA, std::vector<Net::NetEvent> &eventsB, Pred pred, int timeoutMs = 3000)
	{
		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
		while (!pred())
		{
			if (std::chrono::steady_clock::now() > deadline)
				return false;
			Net::NetEvent ev;
			bool any = false;
			while (a.Pump(ev)) { eventsA.push_back(ev); any = true; }
			while (b.Pump(ev)) { eventsB.push_back(ev); any = true; }
			if (!any)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return true;
	}

	constexpr uint16_t c_testPort = 29571;
}

TEST(NetHost, LoopbackEcho)
{
	auto server = Net::NetHost::CreateServer(c_testPort, 4);
	auto client = Net::NetHost::CreateClient();
	ASSERT_TRUE(server);
	ASSERT_TRUE(client);
	EXPECT_EQ(c_testPort, server->GetPort());

	Net::PeerHandle serverPeer = client->Connect("127.0.0.1", c_testPort, 42);
	ASSERT_NE(Net::InvalidPeer, serverPeer);

	std::vector<Net::NetEvent> serverEvents, clientEvents;
	auto hasConnected = [](const std::vector<Net::NetEvent> &events)
	{
		for (auto &ev : events)
			if (ev.type == Net::NetEvent::Type::Connected)
				return true;
		return false;
	};
	ASSERT_TRUE(PumpUntil(*server, *client, serverEvents, clientEvents, [&] { return hasConnected(serverEvents) && hasConnected(clientEvents); }));

	Net::PeerHandle clientPeer = Net::InvalidPeer;
	for (auto &ev : serverEvents)
	{
		if (ev.type == Net::NetEvent::Type::Connected)
		{
			clientPeer = ev.peer;
			EXPECT_EQ(42u, ev.data);
		}
	}
	ASSERT_NE(Net::InvalidPeer, clientPeer);
	EXPECT_EQ(1u, server->GetPeerCount());

	// client -> server
	const std::vector<uint8_t> hello = { 'h', 'i', 0, 255 };
	client->Send(serverPeer, hello);
	serverEvents.clear();
	ASSERT_TRUE(PumpUntil(*server, *client, serverEvents, clientEvents, [&] { return !serverEvents.empty(); }));
	ASSERT_EQ(Net::NetEvent::Type::Data, serverEvents[0].type);
	EXPECT_EQ(clientPeer, serverEvents[0].peer);
	EXPECT_EQ(hello, serverEvents[0].payload);

	// server -> client via broadcast
	const std::vector<uint8_t> reply(1000, 7);
	server->Broadcast(reply);
	clientEvents.clear();
	ASSERT_TRUE(PumpUntil(*server, *client, serverEvents, clientEvents, [&] { return !clientEvents.empty(); }));
	ASSERT_EQ(Net::NetEvent::Type::Data, clientEvents[0].type);
	EXPECT_EQ(reply, clientEvents[0].payload);

	// graceful disconnect from the client side
	client->Disconnect(serverPeer, 5);
	serverEvents.clear();
	clientEvents.clear();
	auto hasDisconnected = [](const std::vector<Net::NetEvent> &events)
	{
		for (auto &ev : events)
			if (ev.type == Net::NetEvent::Type::Disconnected)
				return true;
		return false;
	};
	ASSERT_TRUE(PumpUntil(*server, *client, serverEvents, clientEvents, [&] { return hasDisconnected(serverEvents) && hasDisconnected(clientEvents); }));
	for (auto &ev : serverEvents)
		if (ev.type == Net::NetEvent::Type::Disconnected)
			EXPECT_EQ(5u, ev.data);
	EXPECT_EQ(0u, server->GetPeerCount());
}

TEST(NetHost, ConnectToNobodyTimesOut)
{
	auto client = Net::NetHost::CreateClient();
	// nothing listens on this port; ENet gives up after its connection timeout
	Net::PeerHandle peer = client->Connect("127.0.0.1", c_testPort + 1);
	ASSERT_NE(Net::InvalidPeer, peer);

	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	bool disconnected = false;
	while (!disconnected && std::chrono::steady_clock::now() < deadline)
	{
		Net::NetEvent ev;
		while (client->Pump(ev))
		{
			if (ev.type == Net::NetEvent::Type::Disconnected)
				disconnected = true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	EXPECT_TRUE(disconnected);
}

TEST(NetHost, PortInUseThrows)
{
	auto first = Net::NetHost::CreateServer(c_testPort + 2, 2);
	EXPECT_THROW(Net::NetHost::CreateServer(c_testPort + 2, 2), std::runtime_error);
}
