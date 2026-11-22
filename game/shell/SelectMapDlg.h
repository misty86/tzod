#pragma once
#include <ui/ScrollView.h>
#include <as/MapCollection.h>

namespace FS
{
	class FileSystem;
}

namespace UI
{
	class ContentButton;
	class ScanlineLayout;
}

class ShellConfig;
class LangCache;
class MapCollection;

class SelectMapDlg final
	: public UI::ScrollView
	, private MapCollectionListener
{
public:
	// allowNewMap adds the "+" tile which reports index -1
	SelectMapDlg(FS::FileSystem &fsRoot, WorldView& worldView, ShellConfig &conf, LangCache &lang, MapCollection &mapCollection, bool allowNewMap = true);

	std::function<void(unsigned int)> eventMapSelected;

private:
	FS::FileSystem& _fs;
	WorldView &_worldView;
	ShellConfig &_conf;
	MapCollection& _mapCollection;
	std::shared_ptr<UI::ScanlineLayout> _mapTiles;
	bool _allowNewMap;

	// MapCollectionListener
	void OnMapAdded(int newMapIndex) override;

	std::shared_ptr<UI::ContentButton> MakeMapTileButton(int mapIndex);
};
