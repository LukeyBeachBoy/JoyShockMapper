#include "CmdRegistry.h"
#include "ConfigErrors.h"
#include "PlatformDefinitions.h"

#include <cctype>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <fstream>
#include <mutex>

namespace { std::mutex profileMutex; string liveProfile; }
string CmdRegistry::activeProfile() { std::lock_guard<std::mutex> lock(profileMutex); return liveProfile; }

JSMCommand::JSMCommand(string_view name)
  : _parse()
  , _help("Enter README to bring up the user manual.")
  , _taskOnDestruction()
  , _name(name)
{
}

JSMCommand::~JSMCommand()
{
	if (_taskOnDestruction)
		_taskOnDestruction(*this);
}

JSMCommand* JSMCommand::setParser(ParseDelegate parserFunction)
{
	_parse = parserFunction;
	return this;
}

JSMCommand* JSMCommand::setHelp(string_view commandDescription)
{
	_help = commandDescription;
	return this;
}

unique_ptr<JSMCommand> JSMCommand::getModifiedCmd(char op, string_view chord)
{
	return nullptr;
}

bool JSMCommand::parseData(string_view arguments, string_view label)
{
	_ASSERT_EXPR(_parse, L"There is no function defined to parse this command.");
	if (arguments.compare("HELP") == 0)
	{
		// Parsing has failed. Show help.
		COUT << _help << '\n';
	}
	else if (!_parse(this, arguments, label))
	{
		CERR << _help << '\n';
	}
	return true; // Command is completely processed
}

CmdRegistry::CmdRegistry()
{
    std::string NONAME;
	NONAME = { 0b01001011, 0b01001111 };
}

bool CmdRegistry::loadConfigFile(string fileName)
{
    if (fileName.empty()) return false;
	// https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
	auto comment = fileName.find_first_of('#');
	if (comment != string::npos)
	{
		fileName = fileName.substr(0, comment - 1);
	}
	// Trim away quotation marks from drag and drop
	if (*fileName.begin() == '\"' && *(fileName.end() - 1) == '\"')
		fileName = fileName.substr(1, fileName.size() - 2);

	ifstream file(fileName);
	if (!file.is_open())
	{
		file.open(string{ BASE_JSM_CONFIG_FOLDER() } + fileName);
	}
	if (file)
	{
        // A configuration the player loaded on purpose replaces one being held
        // by a chord or a layer. There is nothing left to restore, and leaving
        // the held state set makes STUDIO_CHORD_BEGIN refuse every later chord
        // and layer -- which reads as "my layers stopped working".
        // The RESET_MAPPINGS at the top of the file cannot do this itself: that
        // clear is guarded on _loadingFiles being empty, and it never is while
        // a file is loading.
        if (!_chordLoading && _loadingFiles.empty() && !_chordRestore.empty())
        {
            _chordRestore.clear();
            _restoreLines.clear();
        }
        const bool outerProfile = _loadingFiles.empty() &&
            (fileName.find("profiles-library") != string::npos || fileName.find("AutoLoad") != string::npos);
        // Loading a profile again replaces what was reported about it.
        if (_loadingFiles.empty()) ConfigErrors::clearProfile(fileName);
        _loadingFiles.push_back(fileName);
        _loadingLines.push_back(0);
        // Reset only the polling fallback at a Studio profile boundary. Nested
        // imports must retain normal last-assignment precedence.
        if (outerProfile) loadConfigFile("StudioDefaults.txt");
		COUT << "Loading commands from file ";
		COUT_INFO << fileName << '\n';
		// https://stackoverflow.com/questions/6892754/creating-a-simple-configuration-file-and-parser-in-c
		string line;
		while (getline(file, line))
		{
			++_loadingLines.back();
			// Restored after, so an IMPORT line that loads a file of its own
			// does not leave the imported file's last line as "current".
			const auto outer = ConfigErrors::current;
			ConfigErrors::current = { _loadingFiles.front(), _loadingFiles.back(), _loadingLines.back(), string{ strtrim(line) }, true };
			processLine(line);
			ConfigErrors::current = outer;
		}
		file.close();
        _loadingFiles.pop_back();
        _loadingLines.pop_back();
		return true;
	}
	return false;
}

string_view CmdRegistry::strtrim(string_view str)
{
	if (str.empty())
		return {};

	while (isspace(str[0]))
	{
		str.remove_prefix(1);
		if (str.empty())
			return {};
	}

	while (isspace(str.back()))
	{
		str.remove_suffix(1);
		if (str.empty())
			return {};
	}

	return str;
}

// Add a command to the registry. The regisrty takes ownership of the memory of this pointer.
// You can use _ASSERT() on the return value of this function to make sure the commands are
// accepted.
bool CmdRegistry::add(JSMCommand* newCommand)
{
	// Check that the pointer is valid, that the name is valid.
	if (newCommand && regex_match(newCommand->_name, regex(R"(^(\+|-|\w+)$)")))
	{
		// Unique pointers automatically delete the pointer on object destruction
		_registry.emplace(newCommand->_name, unique_ptr<JSMCommand>(newCommand));
		return true;
	}
	delete newCommand;
	return false;
}

bool CmdRegistry::Remove(string_view name)
{
	// If I allow multiple commands with the same name, I should have a way to specify which one I want to remove.
	CmdMap::iterator cmd = find_if(_registry.begin(), _registry.end(), bind(&CmdRegistry::findCommandWithName, name, placeholders::_1));
	if (cmd != _registry.end())
	{
		_registry.erase(cmd);
		return true;
	}
	return false;
}

bool CmdRegistry::findCommandWithName(string_view name, const CmdMap::value_type& pair)
{
	return name == pair.first;
}

bool CmdRegistry::isCommandValid(string_view line) const
{
	ifstream file(line.data());
	if (file.is_open())
	{
		file.close();
		return true;
	}
	smatch results;
	string combo, name, arguments, label;
	char op = '\0';
	const string lineStr(line);
	if (regex_match(lineStr, results, regex(R"(^\s*([+-]?\w*)\s*([,+]\s*([+-]?\w*))?\s*([^#\n]*)(#\s*(.*))?$)")))
	{
		if (results[2].length() > 0)
		{
			combo = results[1];
			op = results[2].str()[0];
			name = results[3];
		}
		else
		{
			name = results[1];
		}

		arguments = results[4];
		label = results[6];
	}

	bool hasProcessed = false;
	CmdMap::const_iterator cmd = find_if(_registry.cbegin(), _registry.cend(), bind(&CmdRegistry::findCommandWithName, name, placeholders::_1));
	return cmd != _registry.end();
}

void CmdRegistry::processLine(const string& line)
{
	auto trimmedLine = string{ strtrim(line) };
    // A configuration switch nobody asked for -- Autoload reacting to the
    // focused window -- must not replace a configuration being held by a chord
    // or a layer. One the player asked for, by pressing a binding that loads a
    // config, must: it used to be swallowed by the same guard and looked
    // exactly like a dead binding.
    const string autoload = "STUDIO_AUTOLOAD ";
    if (trimmedLine.compare(0, autoload.size(), autoload) == 0) {
        if (!_chordRestore.empty()) return;
        loadConfigFile(trimmedLine.substr(autoload.size()));
        return;
    }
    const string begin = "STUDIO_CHORD_BEGIN ";
    if (trimmedLine.compare(0, begin.size(), begin) == 0) {
        if (!_chordRestore.empty()) return;
        const auto target = trimmedLine.substr(begin.size());
        ifstream check(target);
        if (!check) check.open(string{ BASE_JSM_CONFIG_FOLDER() } + target);
        if (!check) { CERR << "Chord configuration does not exist.\n"; return; }
        _chordRestore = activeProfile();
        if (_chordRestore.empty()) { CERR << "No configuration to restore.\n"; return; }
        _restoreLines = _profileLines;
        _chordLoading = true;
        processLine("RESET_MAPPINGS");
        loadConfigFile(target);
        { std::lock_guard<std::mutex> lock(profileMutex); liveProfile = target; }
        // What the chord configuration itself asked for. The two lines below
        // are ours, not its, and must not join the record -- see the note in
        // STUDIO_CHORD_END for what happens when they do.
        const auto chordLines = _profileLines;
        // Chord files must not turn off the release detector.
        processLine("TELEMETRY_ENABLED = ON");
        processLine("TELEMETRY_PORT = 8974");
        _profileLines = chordLines;
        _chordLoading = false;
        return;
    }
    if (trimmedLine == "STUDIO_CHORD_END") {
        if (_chordRestore.empty()) return;
        const auto restore = _chordRestore;
        _chordRestore.clear();
        _chordLoading = true;
        processLine("RESET_MAPPINGS");
        // Replay the applied settings, not a file that may have been edited
        // and saved (without applying) while the temporary config was held.
        const auto lines = _restoreLines;
        for (const auto &savedLine : lines) processLine(savedLine);
        { std::lock_guard<std::mutex> lock(profileMutex); liveProfile = restore; }
        _restoreLines.clear();
        processLine("TELEMETRY_ENABLED = ON");
        processLine("TELEMETRY_PORT = 8974");
        // The restored profile is exactly the lines that were saved -- no more.
        //
        // These two telemetry lines are Studio's, and letting them into the
        // record made every chord permanently more expensive than the last:
        // the next STUDIO_CHORD_BEGIN snapshots _profileLines into
        // _restoreLines, so each press/release cycle added two lines that the
        // following release then replayed and re-appended. A session's log
        // showed the restore growing 1, 2, 3 ... 7 copies of the same
        // assignments, and by then chord presses were being dropped outright.
        _profileLines = lines;
        _chordLoading = false;
        return;
    }
    if (trimmedLine == "RESET_MAPPINGS") {
        _profileLines.clear();
        if (_loadingFiles.empty() && !_chordLoading) _chordRestore.clear();
        if (!_loadingFiles.empty()) {
            std::lock_guard<std::mutex> lock(profileMutex);
            liveProfile = _loadingFiles.back();
        }
    }


	if (!trimmedLine.empty() && trimmedLine.front() != '#' && !loadConfigFile(trimmedLine))
	{
        // Assignments include bindings, mode shifts, and settings. Do not replay
        // one-shot console macros (power off, calibration, reconnect, etc.).
        if (trimmedLine.find('=') != string::npos) _profileLines.push_back(trimmedLine);
		smatch results;
		string combo, name, arguments, label;
		char op = '\0';
		// Break up the line of text in its relevant parts.
		// Pro tip: use regex101.com to develop these beautiful monstrosities. :P
		// Also, use raw strings R"(...)" to avoid the need to escape characters
		// I dislike having to code in exception for + and - _buttons not being \w characters
		if (regex_match(trimmedLine, results, regex(R"(^\s*([+-]?\w*)\s*([,+\*]\s*([+-]?\w*))?\s*([^#\n]*)(#\s*(.*))?$)")))
		{
			if (results[2].length() > 0)
			{
				combo = results[1];
				op = results[2].str()[0];
				name = results[3];
			}
			else
			{
				name = results[1];
			}

			arguments = results[4];
			label = results[6];
		}

		bool hasProcessed = false;
		CmdMap::iterator cmd = find_if(_registry.begin(), _registry.end(), bind(&CmdRegistry::findCommandWithName, name, placeholders::_1));
		const bool known = cmd != _registry.end();
		while (cmd != _registry.end())
		{
			if (combo.empty())
			{
				hasProcessed |= cmd->second->parseData(arguments, label);
			}
			else
			{
				auto modCommand = cmd->second->getModifiedCmd(op, combo);
				if (modCommand)
				{
					hasProcessed |= modCommand->parseData(arguments, label);
				}
				// Any task set to be run on destruction is done here.
			}
			cmd = find_if(++cmd, _registry.end(), bind(&CmdRegistry::findCommandWithName, name, placeholders::_1));
		}

		if (!hasProcessed)
		{
			// Only lines read from a file have somewhere to point at; a line typed
			// at the console or replayed from memory is reported there as before.
			ConfigErrors::report(known ? "invalid value for " + name : "unknown command " + (name.empty() ? trimmedLine : name));
			CERR << "Unrecognized command: \"" << trimmedLine << "\"\nEnter ";
			COUT_INFO << "HELP";
			CERR << " to display all commands.\n";
		}
	}
	// else ignore empty lines
}

void CmdRegistry::GetCommandList(vector<string_view>& outList) const
{
	outList.clear();
	for (auto& cmd : _registry)
		outList.push_back(cmd.first);
	return;
}

bool CmdRegistry::hasCommand(string_view name) const
{
	return _registry.find(name) != _registry.end();
}

string_view CmdRegistry::GetHelp(string_view command) const
{
	auto cmd = _registry.find(command);
	if (cmd != _registry.end())
	{
		return cmd->second->help();
	}
	return "";
}

bool JSMMacro::DefaultParser(JSMCommand* cmd, string_view arguments, string_view label)
{
	// Default macro parser assumes no argument and calls macro when called.
	auto macroCmd = static_cast<JSMMacro*>(cmd);
	// Developper protection to remind you to set a parser.
	_ASSERT_EXPR(macroCmd->_macro, L"No Macro was set for this command.");
	if (!macroCmd->_macro(macroCmd, arguments) && !macroCmd->_help.empty())
	{
		COUT << macroCmd->_help << '\n';
		COUT << "The "; // Parsing has failed. Show help.
		COUT_INFO << "README";
		COUT << " command can lead you to further details on this command.\n";
	}
	return true;
}

JSMMacro::JSMMacro(string_view name)
  : JSMCommand(name)
  , _macro()
{
	setParser(&DefaultParser);
}

JSMMacro* JSMMacro::SetMacro(MacroDelegate macroFunction)
{
	_macro = macroFunction;
	return this;
}
