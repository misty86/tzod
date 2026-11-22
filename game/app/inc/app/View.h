#pragma once
#include <deque>
#include <memory>
#include <string>

struct IRender;
class TzodApp;

namespace FS
{
	class FileSystem;
}

namespace Plat
{
	struct AppWindowCommandClose;
	struct AppWindowInputSink;
	struct Input;
	class ConsoleBuffer;
}

// Optional actions performed right after startup; mainly for testing the network game from the command line
struct TzodViewOptions
{
	std::string hostMap;          // create a network game on this map...
	float autoStartDelay = 0;     // ...and start it after this many seconds
	std::string joinAddress;      // join a network game at host[:port]
	bool netDebugChecksum = false; // turn on the per-tick world checksum verification
};

class TzodView final
{
public:
	TzodView(FS::FileSystem &fs, Plat::ConsoleBuffer &logger, TzodApp &app, Plat::AppWindowCommandClose* cmdClose, TzodViewOptions options = {});
	~TzodView();

	void Step(TzodApp& app, float dt, const Plat::Input& input);
	Plat::AppWindowInputSink& GetAppWindowInputSink();

private:
	std::unique_ptr<struct TzodViewImpl> _impl;
	std::deque<float> _movingAverageWindow;
	std::deque<float> _movingMedianWindow;
};
