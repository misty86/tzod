#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>

class AppState;
class AppConfig;
class DMCampaign;
class MapCollection;
class EditorContext;
struct DMSettings;

namespace FS
{
	class FileSystem;
}

namespace Net
{
	class NetSession;
}

namespace Plat
{
	class ConsoleBuffer;
}

class AppController final
{
public:
	AppController(FS::FileSystem &fs);
	~AppController();
	void Step(AppState &appState, AppConfig &appConfig, float dt, bool *outConfigChanged);
//	void NewGameDM(TzodApp &app, const std::string &mapName, const DMSettings &settings);
	void StartDMCampaignMap(AppState &appState, MapCollection& mapCollection, AppConfig &appConfig, DMCampaign &dmCampaign, unsigned int tier, unsigned int map);
	void PlayCurrentMap(AppState &appState, MapCollection& mapCollection);
#ifdef WITH_NETWORK
	void StartNetworkGame(AppState &appState,
	                      MapCollection &mapCollection,
	                      std::string_view mapName,
	                      const DMSettings &settings,
	                      uint32_t seed,
	                      std::shared_ptr<Net::NetSession> session,
	                      unsigned localPlayerIndex,
	                      float tickRate,
	                      unsigned inputDelay,
	                      bool debugChecksum,
	                      Plat::ConsoleBuffer &logger);
#endif
	void StartNewMapEditor(AppState& appState, MapCollection& mapCollection, int width, int height, std::string_view existingMapNameOptional);
	void SaveAndExitEditor(AppState& appState, MapCollection& mapCollection);

private:
	FS::FileSystem &_fs;

	void SaveCurrentMap(EditorContext &editorContext, MapCollection& mapCollection);
};
