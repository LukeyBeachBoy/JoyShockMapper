#pragma once
#include <mutex>
#include <string>
#include <vector>

// Lines of a configuration JoyShockMapper could not use, with the file and line
// they came from, so JSM Studio can point at the line instead of the player
// digging through the console. Keyed by the profile that was being loaded (the
// outermost file), so loading one profile replaces only its own entries and a
// profile's imports report under it. Read by the telemetry thread.
namespace ConfigErrors {
struct Entry {
  std::string profile;
  std::string file;
  int line = 0;
  std::string text;
  std::string reason;
};

inline std::mutex mutex;
inline std::vector<Entry> entries;
constexpr size_t kMaxEntries = 32;

// The line being processed and where it came from, set by CmdRegistry while it
// reads a file. A command that rejects its value reports against it without
// having to know which file it is in. Inactive for a line typed at the console.
struct Location {
  std::string profile;
  std::string file;
  int line = 0;
  std::string text;
  bool active = false;
};
inline thread_local Location current;

inline void clearProfile(const std::string &profile) {
  std::lock_guard<std::mutex> lock(mutex);
  std::erase_if(entries, [&](const Entry &entry) { return entry.profile == profile; });
}

inline void add(Entry entry) {
  std::lock_guard<std::mutex> lock(mutex);
  // One line can fail more than once (GYRO_SENS is two assignments); the first
  // reason is the one worth showing.
  for (const auto &existing : entries)
    if (existing.profile == entry.profile && existing.file == entry.file && existing.line == entry.line) return;
  if (entries.size() >= kMaxEntries) entries.erase(entries.begin());
  entries.push_back(std::move(entry));
}

inline void report(const std::string &reason) {
  if (!current.active) return;
  add({ current.profile, current.file, current.line, current.text, reason });
}

inline void appendEscaped(std::string &out, const std::string &text) {
  out += '"';
  for (unsigned char c : text) {
    if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c >= 32) out += char(c);
  }
  out += '"';
}

inline std::string json() {
  std::lock_guard<std::mutex> lock(mutex);
  std::string out = "[";
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto &entry = entries[i];
    if (i > 0) out += ',';
    out += "{\"profile\":";
    appendEscaped(out, entry.profile);
    out += ",\"file\":";
    appendEscaped(out, entry.file);
    out += ",\"line\":" + std::to_string(entry.line) + ",\"text\":";
    appendEscaped(out, entry.text);
    out += ",\"reason\":";
    appendEscaped(out, entry.reason);
    out += '}';
  }
  return out + "]";
}
} // namespace ConfigErrors
