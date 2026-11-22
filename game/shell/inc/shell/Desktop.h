#pragma once
#include "detail/ConfigConsoleHistory.h"
#include <as/AppStateListener.h>
#include <render/RenderScheme.h>
#include <render/WorldView.h>
#include <ui/Navigation.h>
#include <ui/Window.h>
#include <functional>
#include <string>

namespace FS
{
	class FileSystem;
}

namespace Plat
{
	struct AppWindowCommandClose;
}

namespace UI
{
	class Console;
	class StackLayout;
	class Button;
	class Text;
}

class AppConfig;
class AppController;
class GameLayout;
class FpsCounter;
class ShellConfig;
class LangCache;
class LuaConsole;
class MainMenuDlg;
class MapCollection;
class NavStack;
class JoinGameDlg;

namespace Net
{
	class NetSession;
	struct StartGameMsg;
}

// Optional actions performed right after startup; handy for testing the network game from the command line
struct DesktopStartupOptions
{
	std::string hostMap;       // create a network game on this map...
	float autoStartDelay = 0;  // ...and start it after this many seconds (0 = wait for the Start button)
	std::string joinAddress;   // join a network game at host[:port]
};

class Desktop final
	: public UI::WindowContainer
	, private UI::Managerful
	, private UI::KeyboardSink
	, private UI::NavigationSink
	, private AppStateListener
{
public:
	Desktop(UI::TimeStepManager &manager,
	        TextureManager &texman,
	        AppState &appState,
	        MapCollection &mapCollection,
	        AppConfig &appConfig,
	        AppController &appController,
	        FS::FileSystem &fs,
	        ShellConfig &conf,
	        LangCache &lang,
	        DMCampaign &dmCampaign,
	        Plat::ConsoleBuffer &logger,
	        Plat::AppWindowCommandClose* cmdClose = nullptr,
	        DesktopStartupOptions startupOptions = {});
	virtual ~Desktop();

	void ShowConsole(bool show);

	vec2d GetListenerPos() const;

	// UI::Window
	UI::WindowLayout GetChildLayout(TextureManager &texman, const UI::LayoutContext &lc, const UI::DataContext &dc, const UI::Window &child) const override;
	std::shared_ptr<const UI::Window> GetFocus(const std::shared_ptr<const UI::Window>& owner) const override;
	const UI::Window* GetFocus() const override;

protected:
	UI::NavigationSink* GetNavigationSink() override { return this; }
	UI::KeyboardSink *GetKeyboardSink() override { return this; }

private:
	ConfigConsoleHistory _history;
	TextureManager &_texman;
	MapCollection& _mapCollection;
	AppConfig &_appConfig;
	AppController &_appController;
	FS::FileSystem &_fs;
	ShellConfig &_conf;
	LangCache &_lang;
	DMCampaign &_dmCampaign;
	Plat::ConsoleBuffer &_logger;
	Plat::AppWindowCommandClose* _cmdCloseAppWindow;
	std::unique_ptr<LuaConsole> _luaConsole;
	bool _consoleKeyPressed = false;

	std::shared_ptr<GameLayout> _game;
	std::shared_ptr<UI::Text> _tierTitle;
	std::shared_ptr<UI::Rectangle> _background;
	std::shared_ptr<UI::Console> _con;
	std::shared_ptr<FpsCounter> _fps;
	std::shared_ptr<UI::Button> _pauseButton;
	std::shared_ptr<UI::Button> _backButton;
	std::shared_ptr<NavStack> _navStack;
    std::shared_ptr<UI::StackLayout> _graphs;

	RenderScheme _renderScheme;
	WorldView _worldView;

	void OnNewCampaign();
	void OnSinglePlayer();
	void OnSplitScreen();
#ifdef WITH_NETWORK
	void OnCreateGame();
	void OnJoinGame();
	void HostGameOnMap(std::string mapName, float autoStartDelay);
	std::shared_ptr<JoinGameDlg> ShowJoinGameDlg();
	// Verifies the map and starts the lockstep game; returns false if the game could not be started
	bool StartNetworkGame(std::shared_ptr<Net::NetSession> session, const Net::StartGameMsg &msg, unsigned localPlayerIndex);
#endif
	void OnOpenMap();
	void OnExportMap();
	void OnSettingsMain();
	void OnPlayerSettings();
	void OnControlsSettings();
	void OnAdvancedSettings();
	void OnMapSettings();
	void ShowMainMenu();

	void OnChangeShowFps();

	void OnCommand(std::string_view cmd);
	bool OnCompleteCommand(std::string_view cmd, int &pos, std::string &result);

	void OnCloseChild(std::shared_ptr<UI::Window> child);

	void NavigateBack();
	bool CanNavigateBack() const;

	// AppStateListener
	void OnGameContextRemoving() override;
	void OnGameContextRemoved() override;
	void OnGameContextAdded() override;

	// UI::KeyboardSink
	bool OnKeyPressed(const Plat::Input &input, const UI::InputContext &ic, Plat::Key key) override;
	void OnKeyReleased(const UI::InputContext &ic, Plat::Key key) override;

	// UI::NavigationSink
	bool CanNavigate(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, UI::Navigate navigate) const override;
	void OnNavigate(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, UI::Navigate navigate, UI::NavigationPhase phase) override;
};
