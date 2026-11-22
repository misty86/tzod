// Every shipped map must let a default player spawn; a network game is useless otherwise.

#include <as/MapCollection.h>
#include <ctx/AppConfig.h>
#include <ctx/GameContext.h>
#include <gc/Player.h>
#include <gc/Vehicle.h>
#include <gc/World.h>
#ifdef _WIN32
#include <fswin/FileSystemWin32.h>
using TestFileSystem = FS::FileSystemWin32;
#else
#include <fsposix/FileSystemPosix.h>
using TestFileSystem = FS::FileSystemPosix;
#endif
#include <gtest/gtest.h>
#include <filesystem>

namespace
{
	std::shared_ptr<FS::FileSystem> MakeDataFileSystem()
	{
		auto fs = std::make_shared<TestFileSystem>(TZOD_DATA_DIR);
		auto userDir = std::filesystem::temp_directory_path() / "tzod_net_tests_user";
		std::filesystem::create_directories(userDir);
		fs->Mount("user", std::make_shared<TestFileSystem>(userDir.string()));
		return fs;
	}

	DMSettings DefaultSettings()
	{
		DMSettings settings;
		settings.players.push_back(PlayerDesc{ "Player", "neutral", "default", 0 });
		settings.players.push_back(PlayerDesc{ "Guest", "neutral", "default", 0 });
		settings.bots.push_back(PlayerDesc{ "Bot", "red", "default", 0 });
		settings.timeLimit = 7 * 60;
		settings.fragLimit = 21;
		return settings;
	}
}

TEST(Spawn, PlayersSpawnOnEveryMap)
{
	auto fs = MakeDataFileSystem();
	MapCollection maps(*fs);
	AppConfig appConfig;
	bool configChanged = false;

	for (int mapIndex = 0; mapIndex < maps.GetMapCount(); ++mapIndex)
	{
		std::string mapName(maps.GetMapName(mapIndex));
		GameContext ctx(maps.ExtractCachedWorld(*fs, mapName), DefaultSettings(), 12345);
		// the UI marks local players active when the window is focused; without that the world does not step
		for (auto player : ctx.GetWorldController().GetLocalPlayers())
			player->SetIsActive(true);
		for (int i = 0; i < 300; ++i) // 5 seconds, respawn delay is 2
			ctx.Step(1.0f / 60, appConfig, &configChanged);

		for (size_t p = 0; p < ctx.GetDMPlayerCount(); ++p)
		{
			auto player = ctx.GetDMPlayer(p);
			ASSERT_TRUE(player);
			EXPECT_TRUE(player->GetVehicle() != nullptr || player->GetNumDeaths() > 0)
				<< "map '" << mapName << "', player '" << player->GetNick() << "' never spawned";
		}
	}
}

