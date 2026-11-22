#pragma once
#include <as/AppStateListener.h>
#include <memory>

enum class SoundTemplate;
class SoundHarness;
struct SoundRender;

namespace FS
{
	class FileSystem;
}

namespace Plat
{
	class ConsoleBuffer;
}

class SoundView final
	: private AppStateListener
{
public:
	SoundView(FS::FileSystem &fs, Plat::ConsoleBuffer &logger, AppState &appState);
	~SoundView();
	void Step(vec2d listenerPos);
	void SetVolume(float volume); // 0..1

private:
	std::unique_ptr<SoundRender> _soundRender;
	std::unique_ptr<SoundHarness> _soundHarness;
	float _volume = -1;
	void LoadBuffer(FS::FileSystem &fs, Plat::ConsoleBuffer &logger, SoundTemplate st, const char *fileName);

	// AppStateListener
	void OnGameContextRemoving() override;
	void OnGameContextRemoved() override;
	void OnGameContextAdded() override;
};
