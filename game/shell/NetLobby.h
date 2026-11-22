#pragma once
#include <net/NetSession.h>
#include <ui/StackLayout.h>
#include <ui/Window.h>
#include <functional>
#include <memory>
#include <string>

class AppConfig;
class ConfPlayerBase;
class ShellConfig;
class LangCache;

namespace Plat
{
	class ConsoleBuffer;
}

namespace UI
{
	class Button;
	class Edit;
	class Text;
}

namespace lobby_detail
{
	class MutableText;
}

Net::NetPlayerDesc MakeNetPlayerDesc(const ConfPlayerBase &p);

// Host side lobby: shows who has joined, lets the host add bots and start the game
class CreateGameDlg final
	: public UI::StackLayout
	, private UI::TimeStepping
	, private Net::SessionHandler
{
public:
	CreateGameDlg(UI::TimeStepManager &manager, AppConfig &appConfig, ShellConfig &conf, LangCache &lang, std::shared_ptr<Net::HostSession> session);
	~CreateGameDlg();

	std::function<void(std::shared_ptr<Net::HostSession>, const Net::StartGameMsg&)> eventStart;

	// Presses Start automatically after the delay (testing aid)
	void SetAutoStart(float delaySeconds) { _autoStartDelay = delaySeconds; }

	// UI::TimeStepping
	void OnTimeStep(const Plat::Input &input, bool focused, float dt) override;

private:
	AppConfig &_appConfig;
	ShellConfig &_conf;
	LangCache &_lang;
	std::shared_ptr<Net::HostSession> _session;
	std::shared_ptr<lobby_detail::MutableText> _statusText;
	std::shared_ptr<UI::StackLayout> _players;
	unsigned _botCounter = 0;
	float _autoStartDelay = 0;

	void RefreshPlayers(const Net::LobbyStateMsg &lobby);
	void OnAddBot();
	void OnStart(bool force); // force skips the "everyone is ready" check (testing aid)

	// Net::SessionHandler
	void OnLobbyChanged(const Net::LobbyStateMsg &lobby) override;
};

// Client side: address box, connect button and the lobby as seen from the host
class JoinGameDlg final
	: public UI::StackLayout
	, private UI::TimeStepping
	, private Net::SessionHandler
{
public:
	JoinGameDlg(UI::TimeStepManager &manager, AppConfig &appConfig, ShellConfig &conf, LangCache &lang, Plat::ConsoleBuffer &logger);
	~JoinGameDlg();

	std::function<void(std::shared_ptr<Net::ClientSession>, const Net::StartGameMsg&)> eventStart;

	// Same as typing the address and pressing Connect
	void Connect(std::string address);

	// UI::TimeStepping
	void OnTimeStep(const Plat::Input &input, bool focused, float dt) override;

private:
	AppConfig &_appConfig;
	ShellConfig &_conf;
	LangCache &_lang;
	Plat::ConsoleBuffer &_logger;
	std::shared_ptr<UI::Edit> _address;
	std::shared_ptr<UI::Button> _connectButton;
	std::shared_ptr<UI::Button> _readyButton;
	std::shared_ptr<lobby_detail::MutableText> _statusText;
	std::shared_ptr<UI::StackLayout> _players;
	std::shared_ptr<Net::ClientSession> _session;
	std::unique_ptr<Net::StartGameMsg> _pendingStart;

	void OnConnect();
	void RefreshPlayers(const Net::LobbyStateMsg &lobby);
	void UpdateStatus();

	// Net::SessionHandler
	void OnLobbyChanged(const Net::LobbyStateMsg &lobby) override;
	void OnStartGame(const Net::StartGameMsg &msg) override;
	void OnDisconnected(const std::string &reason) override;
};
