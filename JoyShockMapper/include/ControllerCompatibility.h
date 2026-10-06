#pragma once
#include <string>
#include <vector>
#include <set>
#include <regex>
#include <algorithm>
#include <cctype>

namespace ControllerCompatibility {
inline std::string trim(std::string value) {
    auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
inline std::string key(const std::string &line) {
    auto result = trim(line.substr(0, line.find('=')));
    result.erase(std::remove_if(result.begin(), result.end(), [](unsigned char c){return std::isspace(c);}), result.end());
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c){return std::toupper(c);});
    return result;
}
inline std::string translateToken(std::string token, bool left) {
    const std::string side = left ? "LEFT_" : "RIGHT_";
    if (token == side + "GRID_REQUIRES_CLICK") return "TOUCHPAD_GRID_REQUIRES_CLICK";
    if (token.rfind(side, 0) == 0 && (token.rfind(side + "TOUCH", 0) == 0 || token.rfind(side + "GRID_", 0) == 0)) return token.substr(side.size());
    if (std::regex_match(token, std::regex(left ? "LT[0-9]+" : "RT[0-9]+"))) return token.substr(1);
    if (token == (left ? "MISC3" : "MISC2")) return "CAPTURE";
    if (left && token == "MISC4") return "TOUCH";
    return token;
}
inline std::string translateKey(const std::string &value, bool left) {
    std::string result;
    std::regex tokens("[A-Z][A-Z0-9_]*");
    std::size_t end = 0;
    for (auto it = std::sregex_iterator(value.begin(), value.end(), tokens); it != std::sregex_iterator(); ++it) {
        result += value.substr(end, it->position() - end) + translateToken(it->str(), left);
        end = it->position() + it->length();
    }
    return result + value.substr(end);
}
inline std::vector<std::string> fallback(const std::vector<std::string> &lines, bool left, bool forcePad = false) {
    std::set<std::string> explicitKeys;
    for (const auto &line : lines) if (line.find('=') != std::string::npos && trim(line).rfind('#', 0) != 0) explicitKeys.insert(key(line));
    std::vector<std::string> result;
    for (const auto &line : lines) {
        const auto equals = line.find('=');
        if (equals == std::string::npos || trim(line).rfind('#', 0) == 0) continue;
        const auto original = key(line), translated = translateKey(original, left);
        if (translated != original && (forcePad || !explicitKeys.count(translated))) result.push_back(translated + " = " + line.substr(equals + 1));
    }
    return result;
}
}
