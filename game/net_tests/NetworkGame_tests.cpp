// End-to-end lockstep test: a host and a client in one process talk over the loopback
// interface, simulate the same map and must stay in sync tick by tick.

#include <as/MapCollection.h>
#include <ctx/AppConfig.h>
#include <ctx/WorldController.h>
#include <gc/Player.h>
#include <gc/Vehicle.h>
#include <gc/World.h>
#include <net/NetSession.h>
#include <net/NetworkGameContext.h>
#include <net/WorldChecksum.h>
#include <plat/ConsoleBuffer.h>
#ifdef _WIN32
#include <fswin/FileSystemWin32.h>
using TestFileSystem = FS::FileSystemWin32;
#else
#include <fsposix/FileSystemPosix.h>
using TestFileSystem = FS::FileSystemPosix;
#endif
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <thread>

namespace
{
	constexpr uint16_t c_port = 29580;
	constexpr float c_tickRate = 60;
	constexpr unsigned c_inputDelay = 3;
	constexpr float c_dt = 1.0f / c_tickRate;
	constexpr const char *c_mapName = "dm1";

	std::shared_ptr<FS::FileSystem> MakeDataFileSystem()
	{
		auto fs = std::make_shared<TestFileSystem>(TZOD_DATA_DIR);
		auto userDir = std::filesystem::temp_directory_path() / "tzod_net_tests_user";
		std::filesystem::create_directories(userDir);
		fs->Mount("user", std::make_shared<TestFileSystem>(userDir.string()));
		return fs;
	}

	struct StartCatcher final : Net::SessionHandler
	{
		bool started = false;
		Net::StartGameMsg msg;
		void OnStartGame(const Net::StartGameMsg &m) override { started = true; msg = m; }
	};

	// Pumps the sessions until the predicate holds
	template <class Pred>
	bool PumpUntil(Net::NetSession &a, Net::NetSession &b, Pred pred, int timeoutMs = 5000)
	{
		auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
		while (!pred())
		{
			if (std::chrono::steady_clock::now() > deadline)
				return false;
			a.Pump();
			b.Pump();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return true;
	}

	bool LogContains(const Plat::ConsoleBuffer &logger, const char *needle)
	{
		for (size_t i = 0; i < logger.GetLineCount(); ++i)
		{
			if (std::string(logger.GetLine(i)).find(needle) != std::string::npos)
				return true;
		}
		return false;
	}

	WorldController::ControllerStateMap LocalInput(NetworkGameContext &ctx, float gas, float steerX, bool attack)
	{
		WorldController::ControllerStateMap states;
		if (auto player = ctx.GetDMPlayer(ctx.GetLocalPlayerIndex()))
		{
			if (auto vehicle = player->GetVehicle())
			{
				VehicleState vs = Net::NeutralVehicleState();
				vs.gas = gas;
				vs.steering = vec2d{ steerX, 1 };
				vs.attack = attack;
				states[vehicle->GetId()] = vs;
			}
		}
		return states;
	}
}

TEST(NetworkGame, HostAndClientStayInSync)
{
	Plat::ConsoleBuffer logger(200, 2000);
	auto fs = MakeDataFileSystem();
	MapCollection maps(*fs);

	auto host = std::make_shared<Net::HostSession>(c_port, Net::NetPlayerDesc{ "host", "red", "default", 0 }, logger);
	auto client = std::make_shared<Net::ClientSession>("127.0.0.1", c_port, Net::NetPlayerDesc{ "guest", "blue", "default", 0 }, logger);

	ASSERT_TRUE(PumpUntil(*host, *client, [&] { return client->GetState() == Net::ClientSession::State::InLobby; }));
	EXPECT_EQ(1u, client->GetMyPlayerIndex());
	ASSERT_EQ(2u, host->GetLobby().humans.size());

	host->SetMap(c_mapName, 0);
	host->AddBot(Net::NetPlayerDesc{ "bot1", "FBI Tank", "default", 0 });
	host->AddBot(Net::NetPlayerDesc{ "bot2", "FBI Tank", "default", 0 });
	host->SetRules(1, 0, 0);
	ASSERT_TRUE(PumpUntil(*host, *client, [&] { return client->GetLobby().bots.size() == 2; }));

	// the client reports ready; the host sees it and so does the client through the lobby broadcast
	EXPECT_FALSE(host->GetLobby().IsEveryoneReady());
	client->SetReady(true);
	ASSERT_TRUE(PumpUntil(*host, *client, [&] { return host->GetLobby().IsEveryoneReady() && client->IsReady(); }));

	StartCatcher catcher;
	client->SetHandler(&catcher);
	Net::StartGameMsg startMsg = host->StartGame(777, c_tickRate, c_inputDelay, true /*debugChecksum*/);
	ASSERT_TRUE(PumpUntil(*host, *client, [&] { return catcher.started; }));
	EXPECT_EQ(777u, catcher.msg.seed);
	EXPECT_EQ(2u, catcher.msg.lobby.bots.size());

	DMSettings settings = MakeDMSettings(startMsg.lobby);
	NetworkGameContext hostCtx(maps.ExtractCachedWorld(*fs, c_mapName), settings, startMsg.seed, host, 0, c_tickRate, c_inputDelay, true, logger);
	NetworkGameContext clientCtx(maps.ExtractCachedWorld(*fs, c_mapName), MakeDMSettings(catcher.msg.lobby), catcher.msg.seed, client, 1, c_tickRate, c_inputDelay, true, logger);

	EXPECT_EQ(1u, hostCtx.GetWorldController().GetLocalPlayers().size());
	EXPECT_EQ(1u, clientCtx.GetWorldController().GetLocalPlayers().size());
	EXPECT_EQ(2u, hostCtx.GetWorldController().GetHumanPlayers().size());
	EXPECT_EQ(2u, hostCtx.GetWorldController().GetAIPlayers().size());

	AppConfig appConfig;
	bool configChanged = false;

	// 20 seconds of game time; players drive around and shoot, bots are driven by the host
	const uint32_t targetTicks = 1200;
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
	while (hostCtx.GetTick() < targetTicks || clientCtx.GetTick() < targetTicks)
	{
		ASSERT_LT(std::chrono::steady_clock::now(), deadline) << "host tick " << hostCtx.GetTick() << ", client tick " << clientCtx.GetTick();

		float phase = static_cast<float>(hostCtx.GetTick() % 240) / 240.f;
		hostCtx.SetLocalControllerStates(LocalInput(hostCtx, 1, phase - 0.5f, (hostCtx.GetTick() / 30) % 2 == 0));
		clientCtx.SetLocalControllerStates(LocalInput(clientCtx, -1, 0.5f - phase, (clientCtx.GetTick() / 45) % 2 == 0));

		// a peer that has reached the target keeps pumping the network like the real game loop would
		if (hostCtx.GetTick() < targetTicks)
			hostCtx.Step(c_dt, appConfig, &configChanged);
		else
			host->Pump();
		if (clientCtx.GetTick() < targetTicks)
			clientCtx.Step(c_dt, appConfig, &configChanged);
		else
			client->Pump();

		// the client can never run ahead of the host
		EXPECT_LE(clientCtx.GetTick(), hostCtx.GetTick());

		std::this_thread::sleep_for(std::chrono::microseconds(200));
	}

	EXPECT_EQ(targetTicks, hostCtx.GetTick());
	EXPECT_EQ(targetTicks, clientCtx.GetTick());
	EXPECT_FALSE(hostCtx.IsConnectionLost());
	EXPECT_FALSE(clientCtx.IsConnectionLost());
	EXPECT_FALSE(LogContains(logger, "DESYNC"));

	// identical world state on both sides
	EXPECT_EQ(hostCtx.GetWorld().GetTime(), clientCtx.GetWorld().GetTime());
	EXPECT_EQ(Net::ComputeWorldChecksum(hostCtx.GetWorld()), Net::ComputeWorldChecksum(clientCtx.GetWorld()));

	// something actually happened: the players respawned and the tanks moved
	auto hostPlayer = hostCtx.GetDMPlayer(0);
	auto clientPlayer = clientCtx.GetDMPlayer(1);
	ASSERT_TRUE(hostPlayer && clientPlayer);
	EXPECT_TRUE(hostPlayer->GetVehicle() || hostPlayer->GetNumDeaths() > 0);
	EXPECT_TRUE(clientPlayer->GetVehicle() || clientPlayer->GetNumDeaths() > 0);
}

TEST(NetworkGame, ClientSeesHostLeave)
{
	Plat::ConsoleBuffer logger(200, 500);
	auto fs = MakeDataFileSystem();
	MapCollection maps(*fs);

	auto host = std::make_shared<Net::HostSession>(c_port + 1, Net::NetPlayerDesc{ "host", "red", "default", 0 }, logger);
	auto client = std::make_shared<Net::ClientSession>("127.0.0.1", c_port + 1, Net::NetPlayerDesc{ "guest", "blue", "default", 0 }, logger);
	ASSERT_TRUE(PumpUntil(*host, *client, [&] { return client->GetState() == Net::ClientSession::State::InLobby; }));
	host->SetMap(c_mapName, 0);

	StartCatcher catcher;
	client->SetHandler(&catcher);
	Net::StartGameMsg startMsg = host->StartGame(1, c_tickRate, c_inputDelay, false);
	ASSERT_TRUE(PumpUntil(*host, *client, [&] { return catcher.started; }));

	auto hostCtx = std::make_unique<NetworkGameContext>(maps.ExtractCachedWorld(*fs, c_mapName), MakeDMSettings(startMsg.lobby), startMsg.seed, host, 0, c_tickRate, c_inputDelay, false, logger);
	NetworkGameContext clientCtx(maps.ExtractCachedWorld(*fs, c_mapName), MakeDMSettings(catcher.msg.lobby), catcher.msg.seed, client, 1, c_tickRate, c_inputDelay, false, logger);

	AppConfig appConfig;
	bool configChanged = false;
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	while (clientCtx.GetTick() < 60)
	{
		ASSERT_LT(std::chrono::steady_clock::now(), deadline);
		hostCtx->Step(c_dt, appConfig, &configChanged);
		clientCtx.Step(c_dt, appConfig, &configChanged);
		std::this_thread::sleep_for(std::chrono::microseconds(200));
	}

	// the host goes away; its session disconnects everybody on destruction
	hostCtx.reset();
	host.reset();

	deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (!clientCtx.IsConnectionLost())
	{
		ASSERT_LT(std::chrono::steady_clock::now(), deadline);
		clientCtx.Step(c_dt, appConfig, &configChanged);
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	EXPECT_TRUE(clientCtx.IsGameOver());
	EXPECT_FALSE(clientCtx.IsWorldActive());
	EXPECT_EQ(Net::DisconnectReason::HostLeft, client->GetDisconnectReason());
}
