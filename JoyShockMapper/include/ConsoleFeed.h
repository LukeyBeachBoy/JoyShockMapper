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
  for (size_t i = 0; i < tail.size(); ++i) {
    const auto c = static_cast<unsigned char>(tail[i]);
    if (c == '\n') result += "\\n";
    else if (c == '\r') result += "\\r";
    else if (c == '\t') result += "\\t";
    else if (c == '"') result += "\\\"";
    else if (c == '\\') result += "\\\\";
    else if (c >= 128) {
      // A Windows console/error message can contain an ANSI byte, or the
      // bounded tail can start inside a UTF-8 sequence. Never let that make
      // the entire controller telemetry packet invalid JSON/UTF-8.
      const size_t length = c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
      bool valid = length != 0 && i + length <= tail.size();
      for (size_t n = 1; valid && n < length; ++n) {
        const auto next = static_cast<unsigned char>(tail[i+n]);
        valid = next >= 0x80 && next <= 0xbf;
        if (n == 1) {
          if (c == 0xe0) valid = valid && next >= 0xa0;
          if (c == 0xed) valid = valid && next <= 0x9f;
          if (c == 0xf0) valid = valid && next >= 0x90;
          if (c == 0xf4) valid = valid && next <= 0x8f;
        }
      }
      if (valid) { result.append(tail, i, length); i += length - 1; }
      else result += "\\ufffd";
    }
    else if (c >= 32) result += char(c);
  }
  return result + "\"";
}
}
