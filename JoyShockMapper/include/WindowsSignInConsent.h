#pragma once
#include <fstream>
#include <string>

// A separate per-user preference, never a command in an imported profile.
// Missing, unreadable, truncated or unknown versions fail closed.
inline bool readWindowsSignInConsent(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    char value[12]{};
    file.read(value, sizeof(value));
    return file.gcount() == 11 && std::string(value, 11) == "enabled-v1\n";
}
