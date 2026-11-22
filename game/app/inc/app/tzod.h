#pragma once
#include <memory>

namespace FS
{
	class FileSystem;
}

namespace Plat
{
	class ConsoleBuffer;
}

struct EventPump;
struct AppWindow;

class AppConfig;
class AppState;
class AppController;
class ShellConfig;
class LangCache;
class MapCollection;
class DMCampaign;

// Window settings from the shell config (r_fullscreen, r_width, r_height)
struct TzodDisplaySettings
{
	bool fullscreen = false;
	int width = 1024;
	int height = 768;
};

class TzodApp final
{
public:
	TzodApp(FS::FileSystem &fs, Plat::ConsoleBuffer &logger, const char *language = nullptr);
	~TzodApp();

	MapCollection& GetMapCollection();
	AppState& GetAppState();
	AppController& GetAppController();
	AppConfig& GetAppConfig();
	ShellConfig& GetShellConfig();
	LangCache& GetLang();
	DMCampaign &GetDMCampaign();
	TzodDisplaySettings GetDisplaySettings() const;

	void Step(float dt);
	void SaveConfig();

private:
	FS::FileSystem &_fs;
	Plat::ConsoleBuffer &_logger;
	std::unique_ptr<struct TzodAppImpl> _impl;
};
