#include "AutoLoad.h"

// https://stackoverflow.com/a/4119881/1130520 gives us case insensitive equality
static bool iequals(const string& a, const string& b)
{
	return equal(a.begin(), a.end(),
	  b.begin(), b.end(),
	  [](char a, char b)
	  {
		  return tolower(a) == tolower(b);
	  });
}
namespace JSM
{

AutoLoad::AutoLoad(CmdRegistry* commandRegistry, bool start)
  : PollingThread("AutoLoad thread", bind(&AutoLoad::AutoLoadPoll, this, placeholders::_1), (void*)commandRegistry, 1000, start)
{
}

bool AutoLoad::AutoLoadPoll(void* param)
{
	auto registry = reinterpret_cast<CmdRegistry*>(param);
	static string lastModuleName;
	// Module name only here: on win32 this is a kernel-level lookup with no
	// dependency on the foreground process's own responsiveness. This runs
	// every second for the life of the session, so on every one of those polls
	// where the foreground app is unchanged -- the overwhelming majority of
	// them, during actual play -- there is nothing here that can stall.
	//
	// The window TITLE (GetActiveWindowName, below) is fetched only once
	// something has actually changed: it goes through GetWindowText, which on
	// win32 sends WM_GETTEXT to a window owned by another process and can block
	// for as long as that process's own message pump is busy. Calling that
	// unconditionally on this same one-second poll meant the wait -- however
	// long the foreground game took to get around to it -- landed on a fixed,
	// repeating cadence for the entire session: a periodic stall on a thread
	// that shares this process with the one actually emitting mouse motion,
	// which is what surfaced as a "double cursor" teleport, once a second, on
	// every output path (touchpad, gyro, stick aim) alike -- none of which
	// this thread's own code touches at all.
	string windowModule = GetActiveWindowModule();
	if (!windowModule.empty() && windowModule != lastModuleName && windowModule.compare("JoyShockMapper.exe") != 0)
	{
		lastModuleName = windowModule;
		string path(AUTOLOAD_FOLDER());
		auto files = ListDirectory(path);
		auto noextmodule = windowModule.substr(0, windowModule.find_first_of('.'));
		string windowTitle;
		tie(ignore, windowTitle) = GetActiveWindowName();
		COUT_INFO << "[AUTOLOAD] \"" << windowTitle << "\" in focus: "; // looking for config : " , );
		bool success = false;
		for (auto file : files)
		{
			auto noextconfig = file.substr(0, file.find_first_of('.'));
			if (iequals(noextconfig, noextmodule))
			{
				COUT_INFO << "loading \"AutoLoad\\" << noextconfig << ".txt\".\n";
				WriteToConsole(path + file);
				success = true;
				break;
			}
		}
		if (!success)
		{
			COUT_INFO << "create ";
			COUT << "AutoLoad\\" << noextmodule << ".txt";
			COUT_INFO << " to autoload for this application.\n";
		}
	}
	return true;
}

} // namespace JSM