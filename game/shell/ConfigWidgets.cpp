#include "ConfigWidgets.h"
#include "KeyMapper.h"
#include <cbind/ConfigBinding.h>
#include <plat/Keys.h>
#include <ui/Button.h>
#include <ui/Edit.h>
#include <ui/InputContext.h>
#include <ui/Text.h>
#include <ui/EditableText.h>
#include <ui/LayoutContext.h>

StringSetting::StringSetting(ConfVarString& stringVar)
	: _stringVar(stringVar)
{
	std::get<UI::Text>(_children).SetFont("font_default");

	std::get<UI::Edit>(_children).GetEditable().SetText(std::string(stringVar.Get()));
	std::get<UI::Edit>(_children).GetEditable().SetFont("font_default");
	std::get<UI::Edit>(_children).GetEditable().eventChange = [=]
	{
		auto text = std::get<UI::Edit>(_children).GetEditable().GetText();
		if (text.empty())
			_stringVar.Set(_defaultValue);
		else
			_stringVar.Set(std::get<UI::Edit>(_children).GetEditable().GetText());
	};
}

void StringSetting::SetTitle(std::shared_ptr<UI::LayoutData<std::string_view>> title)
{
	std::get<UI::Text>(_children).SetText(std::move(title));
}

UI::WindowLayout StringSetting::GetChildLayout(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, const UI::Window& child) const
{
	if (&std::get<UI::Text>(_children) == &child)
	{
		return UI::WindowLayout{ MakeRectWH(std::floor(lc.GetPixelSize().x / 2), lc.GetPixelSize().y), 1, true };
	}
	else if (&std::get<UI::Edit>(_children) == &child)
	{
		return UI::WindowLayout{ MakeRectRB(vec2d{ std::floor(lc.GetPixelSize().x / 2), 0 }, lc.GetPixelSize()), 1, true };
	}
	assert(false);
	return {};
}

vec2d StringSetting::GetContentSize(TextureManager& texman, const UI::DataContext& dc, float scale, const UI::LayoutConstraints& layoutConstraints) const
{
	return vec2d{ 0, std::get<UI::Edit>(_children).GetContentSize(texman, dc, scale, layoutConstraints).y };
}

///////////////////////////////////////////////////////////////////////////////

BooleanSetting::BooleanSetting(ConfVarBool& boolVar)
	: _boolVar(boolVar)
{
	std::get<0>(_children).SetFont("font_default");
	std::get<0>(_children).SetBoxPosition(UI::CheckBox::BoxPosition::Right);
	std::get<0>(_children).SetCheck(_boolVar.Get());
	std::get<0>(_children).eventClick = [=]
	{
		_boolVar.Set(std::get<0>(_children).GetCheck());
	};
}

void BooleanSetting::SetTitle(std::shared_ptr<UI::LayoutData<std::string_view>> title)
{
	std::get<0>(_children).SetText(title);
}

UI::WindowLayout BooleanSetting::GetChildLayout(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, const UI::Window& child) const
{
	return UI::WindowLayout{ MakeRectWH(lc.GetPixelSize()), 1, true };
}

vec2d BooleanSetting::GetContentSize(TextureManager& texman, const UI::DataContext& dc, float scale, const UI::LayoutConstraints& layoutConstraints) const
{
	return std::get<0>(_children).GetContentSize(texman, dc, scale, layoutConstraints);
}

///////////////////////////////////////////////////////////////////////////////
//#include <ui/StateContext.h>
#include <ui/DataSource.h>

static const auto c_textColor = std::make_shared<UI::StateBinding<SpriteColor>>(0xffeeeeee, // default
	UI::StateBinding<SpriteColor>::MapType{ { "Disabled", 0xaaaaaaaa }, { "Hover", 0xffffffff }, { "Pushed", 0xffffffff } });

KeyBindSetting::KeyBindSettingContent::KeyBindSettingContent(const ConfVarString& stringKeyVar)
{
	// title
	std::get<0>(_children).SetFont("font_default");
	std::get<0>(_children).SetFontColor(c_textColor);

	// value
	std::get<1>(_children).SetFont("font_default");
	std::get<1>(_children).SetText(ConfBind(stringKeyVar));
	std::get<1>(_children).SetAlign(alignTextRT);
	std::get<1>(_children).SetFontColor(c_textColor);
}

void KeyBindSetting::KeyBindSettingContent::SetTitle(std::shared_ptr<UI::LayoutData<std::string_view>> title)
{
	// title
	std::get<0>(_children).SetText(std::move(title));
}

// UI::Window
UI::WindowLayout KeyBindSetting::KeyBindSettingContent::GetChildLayout(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, const UI::Window& child) const
{
	if (&std::get<0>(_children) == &child)
	{
		return UI::WindowLayout{ MakeRectWH(std::floor(lc.GetPixelSize().x / 2), lc.GetPixelSize().y), 1, true };
	}
	else if (&std::get<1>(_children) == &child)
	{
		return UI::WindowLayout{ MakeRectRB(vec2d{ lc.GetPixelSize().x, 0 }, lc.GetPixelSize()), 1, true };
	}
	assert(false);
	return{};
}

vec2d KeyBindSetting::KeyBindSettingContent::GetContentSize(TextureManager& texman, const UI::DataContext& dc, float scale, const UI::LayoutConstraints& layoutConstraints) const
{
	vec2d pxTitleSize = std::get<0>(_children).GetContentSize(texman, dc, scale, layoutConstraints);
	vec2d pxValueSize = std::get<1>(_children).GetContentSize(texman, dc, scale, layoutConstraints);
	return vec2d{ pxTitleSize.x + pxValueSize.x, std::max(pxTitleSize.y, pxValueSize.y) };
}

KeyBindSetting::KeyBindSetting(ConfVarString& stringKeyVar)
	: _stringKeyVar(stringKeyVar)
	, _content(std::make_shared<KeyBindSettingContent>(stringKeyVar))
{
	std::get<0>(_children).SetContent(_content);
	std::get<0>(_children).eventClick = [=]
	{
		_waitingForKey = true;
	};
}

void KeyBindSetting::SetTitle(std::shared_ptr<UI::LayoutData<std::string_view>> title)
{
	_content->SetTitle(std::move(title));
}

UI::WindowLayout KeyBindSetting::GetChildLayout(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, const UI::Window& child) const
{
	return UI::WindowLayout{ MakeRectWH(lc.GetPixelSize()), (lc.GetFocusedCombined() && _waitingForKey) ? 0.5f : 1, true };
}

vec2d KeyBindSetting::GetContentSize(TextureManager& texman, const UI::DataContext& dc, float scale, const UI::LayoutConstraints& layoutConstraints) const
{
	return std::get<0>(_children).GetContentSize(texman, dc, scale, layoutConstraints);
}

bool KeyBindSetting::OnKeyPressed(const Plat::Input &input, const UI::InputContext& ic, Plat::Key key)
{
	if (_waitingForKey)
	{
		if (Plat::Key::Escape != key)
			_stringKeyVar.Set(GetKeyName(key));
		_waitingForKey = false;
		return true;
	}
	else if (Plat::Key::Delete == key)
	{
		_stringKeyVar.Set("");
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

#include <ui/Navigation.h>
#include <algorithm>

namespace
{
	constexpr int c_percentMax = 10000;
	constexpr int c_percentStep = 1000;

	class PercentBinding final
		: public UI::LayoutData<std::string_view>
	{
	public:
		explicit PercentBinding(const ConfVarNumber& numberVar) : _numberVar(numberVar) {}

		// UI::LayoutData<std::string_view>
		std::string_view GetLayoutValue(const UI::DataContext& dc) const override
		{
			_text = std::to_string(std::max(0, std::min(c_percentMax, _numberVar.GetInt())) / 100) + "%";
			return _text;
		}

	private:
		const ConfVarNumber& _numberVar;
		mutable std::string _text;
	};

	// invisible child that turns Left/Right navigation into value changes
	class PercentNavigationProxy final
		: public UI::Window
		, private UI::NavigationSink
	{
	public:
		explicit PercentNavigationProxy(ConfVarNumber& numberVar) : _numberVar(numberVar) {}

		// UI::Window
		UI::NavigationSink* GetNavigationSink() override { return this; }

	private:
		ConfVarNumber& _numberVar;

		// UI::NavigationSink
		bool CanNavigate(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, UI::Navigate navigate) const override
		{
			return UI::Navigate::Left == navigate || UI::Navigate::Right == navigate;
		}
		void OnNavigate(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, UI::Navigate navigate, UI::NavigationPhase phase) override
		{
			if (phase == UI::NavigationPhase::Started)
			{
				int delta = UI::Navigate::Left == navigate ? -c_percentStep : c_percentStep;
				_numberVar.SetInt(std::max(0, std::min(c_percentMax, _numberVar.GetInt() + delta)));
			}
		}
	};
}

class PercentSetting::Content final
	: public UI::WindowContainer
{
public:
	Content(ConfVarNumber& numberVar)
		: _title(std::make_shared<UI::Text>())
		, _value(std::make_shared<UI::Text>())
	{
		_title->SetFont("font_default");
		_title->SetFontColor(c_textColor);
		AddFront(_title);

		_value->SetFont("font_default");
		_value->SetText(std::make_shared<PercentBinding>(numberVar));
		_value->SetAlign(alignTextRT);
		_value->SetFontColor(c_textColor);
		AddFront(_value);

		auto proxy = std::make_shared<PercentNavigationProxy>(numberVar);
		AddFront(proxy);
		SetFocus(proxy.get());
	}

	void SetTitle(std::shared_ptr<UI::LayoutData<std::string_view>> title)
	{
		_title->SetText(std::move(title));
	}

	// UI::Window
	UI::WindowLayout GetChildLayout(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, const UI::Window& child) const override
	{
		if (_title.get() == &child)
			return UI::WindowLayout{ MakeRectWH(std::floor(lc.GetPixelSize().x / 2), lc.GetPixelSize().y), 1, true };
		if (_value.get() == &child)
			return UI::WindowLayout{ MakeRectRB(vec2d{ lc.GetPixelSize().x, 0 }, lc.GetPixelSize()), 1, true };
		return UI::WindowLayout{ MakeRectWH(lc.GetPixelSize()), 1, true }; // navigation proxy, invisible
	}

	vec2d GetContentSize(TextureManager& texman, const UI::DataContext& dc, float scale, const UI::LayoutConstraints& layoutConstraints) const override
	{
		vec2d pxTitleSize = _title->GetContentSize(texman, dc, scale, layoutConstraints);
		vec2d pxValueSize = _value->GetContentSize(texman, dc, scale, layoutConstraints);
		return vec2d{ pxTitleSize.x + pxValueSize.x, std::max(pxTitleSize.y, pxValueSize.y) };
	}

private:
	std::shared_ptr<UI::Text> _title;
	std::shared_ptr<UI::Text> _value;
};

PercentSetting::PercentSetting(ConfVarNumber& numberVar)
	: _numberVar(numberVar)
	, _content(std::make_shared<Content>(numberVar))
{
	std::get<0>(_children).SetContent(_content);
	std::get<0>(_children).eventClick = [=]
	{
		// mouse users: cycle through the values
		int next = _numberVar.GetInt() + c_percentStep;
		_numberVar.SetInt(next > c_percentMax ? 0 : next);
	};
}

void PercentSetting::SetTitle(std::shared_ptr<UI::LayoutData<std::string_view>> title)
{
	_content->SetTitle(std::move(title));
}

UI::WindowLayout PercentSetting::GetChildLayout(TextureManager& texman, const UI::LayoutContext& lc, const UI::DataContext& dc, const UI::Window& child) const
{
	return UI::WindowLayout{ MakeRectWH(lc.GetPixelSize()), 1, true };
}

vec2d PercentSetting::GetContentSize(TextureManager& texman, const UI::DataContext& dc, float scale, const UI::LayoutConstraints& layoutConstraints) const
{
	return std::get<0>(_children).GetContentSize(texman, dc, scale, layoutConstraints);
}
