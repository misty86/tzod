#define WIN32_LEAN_AND_MEAN
#include <Shlobj.h>
#include <plat/Folders.h>
#include <utf8.h>

static std::string w2s(std::wstring_view w)
{
	std::string s;
	s.reserve(w.size());
	utf8::utf16to8(w.begin(), w.end(), std::back_inserter(s));
	return s;
}

std::string Plat::GetAppDataFolder()
{
	std::string result;
	PWSTR path = nullptr;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &path);
	if (FAILED(hr))
		throw std::runtime_error("Failed to get AppData folder");
	if (path)
	{
		result = w2s(path);
		CoTaskMemFree(path);
		path = nullptr;
	}
	return result;
}

std::string Plat::GetBundleResourcesFolder()
{
	// Prefer the working directory (developers run from the repo root),
	// otherwise look next to the executable and a few levels above it
	// (build trees keep the exe a few folders below the repo root) so a double-click works too.
	if (GetFileAttributesW(L"data") != INVALID_FILE_ATTRIBUTES)
		return ".";

	wchar_t exePath[MAX_PATH];
	DWORD len = GetModuleFileNameW(NULL, exePath, MAX_PATH);
	if (len > 0 && len < MAX_PATH)
	{
		std::wstring dir(exePath, len);
		for (int level = 0; level < 4; ++level)
		{
			size_t slash = dir.find_last_of(L"\\/");
			if (slash == std::wstring::npos || slash == 0)
				break;
			dir.resize(slash);
			if (GetFileAttributesW((dir + L"\\data").c_str()) != INVALID_FILE_ATTRIBUTES)
				return w2s(dir);
		}
	}
	return ".";
}
