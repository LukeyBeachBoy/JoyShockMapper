#pragma once
#include <mutex>
#include <string>

// Bounded console tail for the local telemetry viewer. No disk writes on input threads.
namespace ConsoleFeed {
inline std::mutex mutex;
inline std::string tail;
inline void append(const std::string &text) {
  std::lock_guard<std::mutex> lock(mutex);
  tail += text;
  if (tail.size() > 4096) {
    tail.erase(0, tail.size() - 4096);
    const auto newline = tail.find('\n');
    if (newline != std::string::npos) tail.erase(0, newline + 1);
  }
}
inline std::string json() {
  std::lock_guard<std::mutex> lock(mutex);
  std::string result = "\"";
  for (unsigned char c : tail) {
    if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (c == '\t') result += "\\t";
    else if (c == '"') result += "\\\"";
    else if (c == '\\') result += "\\\\";
    else if (c >= 32) result += char(c);
  }
  return result + "\"";
}
}
