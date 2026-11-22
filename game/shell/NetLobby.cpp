#include "NetLobby.h"
#include "inc/shell/Config.h"
#include <cbind/ConfigBinding.h>
#include <ctx/AppConfig.h>
#include <loc/Language.h>
#include <plat/ConsoleBuffer.h>
#include <ui/Button.h>
#include <ui/DataSource.h>
#include <ui/Edit.h>
#include <ui/EditableText.h>
#include <ui/LayoutContext.h>
#include <ui/Text.h>
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace lobby_detail
{
	class MutableText final
		: public UI::LayoutData<std::string_view>
	{
	public:
		void Set(std::string text) { _text = std::move(text); }

		// UI::LayoutData<std::string_view>
		std::string_view GetLayoutValue(const UI::DataContext &dc) const override { return _text; }

	private:
		std::string _text;
	};

	// Gives a child without a natural size (e.g. an edit box) a fixed size inside a stack layout
	class FixedSize final
		: public UI::WindowContainer
	{
	public:
		FixedSize(std::shared_ptr<UI::Window> child, vec2d size)
			: _size(size)
		{
			AddFront(child);
			SetFocus(child.get());
		}

		// UI::Window
		vec2d GetContentSize(TextureManager &texman, const UI::DataContext &dc, float scale, const UI::LayoutConstraints &layoutConstraints) const override
		{
			return vec2d{ std::floor(_size.x * scale), std::floor(_size.y * scale) };
		}

		UI::WindowLayout GetChildLayout(TextureManager &texman, const UI::LayoutContext &lc, const UI::DataContext &dc, const UI::Window &child) const override
		{
			return UI::WindowLayout{ MakeRectWH(lc.GetPixelSize()), 1, true };
		}

	private:
		vec2d _size;
	};
}

using lobby_detail::MutableText;
using lobby_detail::FixedSize;

namespace
{
	constexpr float c_buttonWidth = 200;
	constexpr float c_buttonHeight = 50;
	constexpr unsigned c_defaultInputDelay = 3; // ticks

	std::shared_ptr<UI::Text> MakeText(std::shared_ptr<UI::LayoutData<std::string_view>> text, const char *font = "font_small")
	{
		auto result = std::make_shared<UI::Text>();
		result->SetFont(font);
		result->SetText(std::move(text));
		return result;
	}

	std::shared_ptr<UI::Button> MakeButton(const ConfVarString &text, std::function<void()> onClick)
	{
		auto button = std::make_shared<UI::Button>();
		button->SetFont("font_default");
		button->SetText(ConfBind(text));
		button->Resize(c_buttonWidth, c_buttonHeight);
		button->eventClick = std::move(onClick);
		return button;
	}

	std::string FormatPlayerLine(const Net::NetPlayerDesc &pd, bool isBot, bool ready, const LangCache &lang)
	{
		std::ostringstream s;
		s << pd.nick;
		if (isBot)
			s << " (bot)";
		if (pd.team != 0)
			s << " - " << lang.net_chatroom_team.Get() << (int)pd.team;
		if (ready)
			s << " - " << lang.net_chatroom_player_ready.Get();
		return s.str();
	}

	void FillPlayerList(UI::StackLayout &list, const Net::LobbyStateMsg &lobby, const LangCache &lang)
	{
		list.UnlinkAllChildren();
		for (unsigned i = 0; i < lobby.humans.size(); ++i)
			list.AddFront(MakeText(std::make_shared<UI::StaticText>(FormatPlayerLine(lobby.humans[i], false, lobby.IsReady(i), lang))));
		for (auto &pd : lobby.bots)
			list.AddFront(MakeText(std::make_shared<UI::StaticText>(FormatPlayerLine(pd, true, false, lang))));
	}

	std::string Format(std::string_view fmt, const std::string &arg)
	{
		char buf[256];
		snprintf(buf, sizeof(buf), std::string(fmt).c_str(), arg.c_str());
		return buf;
	}
}

Net::NetPlayerDesc MakeNetPlayerDesc(const ConfPlayerBase &p)
{
	Net::NetPlayerDesc result;
	result.nick = p.nick.Get();
	result.skin = p.skin.Get();
	result.cls = p.platform_class.Get();
	result.team = static_cast<uint8_t>(std::max(0, std::min(255, p.team.GetInt())));
	return result;
}

///////////////////////////////////////////////////////////////////////////////

CreateGameDlg::CreateGameDlg(UI::TimeStepManager &manager, AppConfig &appConfig, ShellConfig &conf, LangCache &lang, std::shared_ptr<Net::HostSession> session)
	: UI::TimeStepping(manager)
	, _appConfig(appConfig)
	, _conf(conf)
	, _lang(lang)
	, _session(std::move(session))
	, _statusText(std::make_shared<MutableText>())
	, _players(std::make_shared<UI::StackLayout>())
{
	SetFlowDirection(UI::FlowDirection::Vertical);
	SetSpacing(16);
	SetAlign(UI::Align::CT);

	AddFront(MakeText(ConfBind(_lang.net_server_title), "font_default"));
	AddFront(MakeText(std::make_shared<UI::StaticText>(Format(_lang.net_lobby_map.Get(), _session->GetLobby().mapName))));
	AddFront(MakeText(_statusText));
	AddFront(MakeText(ConfBind(_lang.net_chatroom_players)));

	_players->SetFlowDirection(UI::FlowDirection::Vertical);
	_players->SetSpacing(4);
	_players->SetAlign(UI::Align::CT);
	AddFront(_players);

	auto addBot = MakeButton(_lang.net_chatroom_bot_new, std::bind(&CreateGameDlg::OnAddBot, this));
	AddFront(addBot);

	auto start = MakeButton(_lang.net_lobby_start, [this] { OnStart(false); });
	AddFront(start);
	SetFocus(start.get());

	RefreshPlayers(_session->GetLobby());
	_session->SetHandler(this);
	SetTimeStep(true);
}

CreateGameDlg::~CreateGameDlg()
{
	if (_session->GetHandler() == this)
		_session->SetHandler(nullptr);
}

void CreateGameDlg::OnTimeStep(const Plat::Input &input, bool focused, float dt)
{
	_session->Pump();

	if (_autoStartDelay > 0)
	{
		_autoStartDelay -= dt;
		if (_autoStartDelay <= 0)
		{
			_autoStartDelay = 0;
			OnStart(true); // may destroy this dialog; must stay the last statement
		}
	}
}

void CreateGameDlg::RefreshPlayers(const Net::LobbyStateMsg &lobby)
{
	FillPlayerList(*_players, lobby, _lang);

	std::ostringstream status;
	status << Format(_lang.net_lobby_listening.Get(), std::to_string(_session->GetPort()));
	unsigned readyCount = 0;
	for (unsigned i = 0; i < lobby.humans.size(); ++i)
		readyCount += lobby.IsReady(i) ? 1 : 0;
	status << ", " << lobby.humans.size() << " / " << Net::MAX_NET_PLAYERS << ", " << readyCount << " " << _lang.net_lobby_ready_count.Get();
	_statusText->Set(status.str());
}

void CreateGameDlg::OnAddBot()
{
	Net::NetPlayerDesc bot = MakeNetPlayerDesc(_conf.ui_netbotinfo);
	bot.nick = "Bot " + std::to_string(++_botCounter);
	_session->AddBot(std::move(bot));
	RefreshPlayers(_session->GetLobby());
}

void CreateGameDlg::OnStart(bool force)
{
	if (_session->IsGameStarted())
		return;

	if (!force && !_session->GetLobby().IsEveryoneReady())
	{
		_statusText->Set(std::string(_lang.net_lobby_not_ready.Get()));
		return;
	}

	uint32_t seed = static_cast<uint32_t>(rand()) ^ (static_cast<uint32_t>(rand()) << 16);
	float tickRate = std::max(10.f, std::min(120.f, _conf.sv_fps.GetFloat()));
	int latency = _conf.sv_latency.GetInt();
	unsigned inputDelay = latency > 0 ? static_cast<unsigned>(std::min(latency, 30)) : c_defaultInputDelay;

	Net::StartGameMsg msg = _session->StartGame(seed, tickRate, static_cast<uint8_t>(inputDelay), _conf.net_debugchecksum.Get());
	if (eventStart)
		eventStart(_session, msg);
}

void CreateGameDlg::OnLobbyChanged(const Net::LobbyStateMsg &lobby)
{
	RefreshPlayers(lobby);
}

///////////////////////////////////////////////////////////////////////////////

JoinGameDlg::JoinGameDlg(UI::TimeStepManager &manager, AppConfig &appConfig, ShellConfig &conf, LangCache &lang, Plat::ConsoleBuffer &logger)
	: UI::TimeStepping(manager)
	, _appConfig(appConfig)
	, _conf(conf)
	, _lang(lang)
	, _logger(logger)
	, _address(std::make_shared<UI::Edit>())
	, _statusText(std::make_shared<MutableText>())
	, _players(std::make_shared<UI::StackLayout>())
{
	SetFlowDirection(UI::FlowDirection::Vertical);
	SetSpacing(16);
	SetAlign(UI::Align::CT);

	AddFront(MakeText(ConfBind(_lang.net_connect_title), "font_default"));
	AddFront(MakeText(ConfBind(_lang.net_connect_address)));

	_address->GetEditable().SetText(std::string(_conf.cl_server.Get()));
	auto addressBox = std::make_shared<FixedSize>(_address, vec2d{ 300, 24 });
	AddFront(addressBox);

	_connectButton = MakeButton(_lang.net_connect_ok, std::bind(&JoinGameDlg::OnConnect, this));
	AddFront(_connectButton);

	_readyButton = MakeButton(_lang.net_chatroom_ready_button, [this]
	{
		if (_session)
			_session->SetReady(!_session->IsReady());
	});
	_readyButton->SetVisible(false);
	AddFront(_readyButton);

	AddFront(MakeText(_statusText));

	_players->SetFlowDirection(UI::FlowDirection::Vertical);
	_players->SetSpacing(4);
	_players->SetAlign(UI::Align::CT);
	AddFront(_players);

	SetFocus(addressBox.get());
	SetTimeStep(true);
}

JoinGameDlg::~JoinGameDlg()
{
	if (_session && _session->GetHandler() == this)
		_session->SetHandler(nullptr);
}

void JoinGameDlg::OnTimeStep(const Plat::Input &input, bool focused, float dt)
{
	if (_session)
	{
		_session->Pump();
		UpdateStatus();
	}

	// Fire the start event last and without touching any member afterwards: the handler
	// closes this dialog, which destroys it right here.
	if (_pendingStart)
	{
		auto session = _session;
		auto msg = std::move(*_pendingStart);
		auto callback = eventStart;
		_pendingStart.reset();
		if (callback)
			callback(std::move(session), msg);
	}
}

void JoinGameDlg::Connect(std::string address)
{
	_logger.Printf(0, "net: JoinGameDlg::Connect(%s)", address.c_str());
	_address->GetEditable().SetText(std::move(address));
	OnConnect();
}

void JoinGameDlg::OnConnect()
{
	std::string address(_address->GetEditable().GetText());
	if (address.empty())
		return;

	std::string hostname = address;
	int port = _conf.sv_port.GetInt();
	if (auto colon = address.rfind(':'); colon != std::string::npos)
	{
		hostname = address.substr(0, colon);
		port = atoi(address.c_str() + colon + 1);
	}
	if (hostname.empty() || port <= 0 || port > 0xffff)
	{
		_statusText->Set(std::string(_lang.net_msg_connection_failed.Get()));
		return;
	}

	_conf.cl_server.Set(address);
	_players->UnlinkAllChildren();

	// drop the previous attempt, if any
	if (_session)
	{
		_session->SetHandler(nullptr);
		_session.reset();
	}

	try
	{
		_session = std::make_shared<Net::ClientSession>(hostname, static_cast<uint16_t>(port), MakeNetPlayerDesc(_appConfig.sp_playerinfo), _logger);
		_session->SetHandler(this);
		UpdateStatus();
	}
	catch (const std::exception &e)
	{
		_logger.Printf(1, "Could not connect - %s", e.what());
		_statusText->Set(std::string(_lang.net_msg_connection_failed.Get()));
	}
}

void JoinGameDlg::RefreshPlayers(const Net::LobbyStateMsg &lobby)
{
	FillPlayerList(*_players, lobby, _lang);
}

void JoinGameDlg::UpdateStatus()
{
	assert(_session);
	_readyButton->SetVisible(_session->GetState() == Net::ClientSession::State::InLobby);
	std::string status;
	switch (_session->GetState())
	{
	case Net::ClientSession::State::Connecting:
		status = _lang.net_msg_connecting.Get();
		break;
	case Net::ClientSession::State::Handshaking:
		status = _lang.net_msg_connection_established.Get();
		break;
	case Net::ClientSession::State::InLobby:
		status = _lang.net_lobby_waiting_for_host.Get();
		break;
	case Net::ClientSession::State::InGame:
		status = _lang.net_msg_starting_game.Get();
		break;
	case Net::ClientSession::State::Disconnected:
		if (_session->WasRejected())
		{
			switch (_session->GetRejectReason())
			{
			case Net::RejectReason::VersionMismatch:
				status = _lang.net_connect_error_server_version.Get();
				break;
			case Net::RejectReason::ServerFull:
				status = _lang.net_connect_error_full.Get();
				break;
			case Net::RejectReason::GameAlreadyStarted:
				status = _lang.net_connect_error_game_started.Get();
				break;
			default:
				status = _session->GetStatusText();
				break;
			}
		}
		else if (_session->GetDisconnectReason() == Net::DisconnectReason::HostLeft)
		{
			status = _lang.net_msg_host_disconnected.Get();
		}
		else
		{
			status = _lang.net_msg_connection_failed.Get();
		}
		break;
	}
	_statusText->Set(std::move(status));
}

void JoinGameDlg::OnLobbyChanged(const Net::LobbyStateMsg &lobby)
{
	RefreshPlayers(lobby);
}

void JoinGameDlg::OnStartGame(const Net::StartGameMsg &msg)
{
	_pendingStart = std::make_unique<Net::StartGameMsg>(msg);
}

void JoinGameDlg::OnDisconnected(const std::string &reason)
{
	_players->UnlinkAllChildren();
	UpdateStatus();
}
