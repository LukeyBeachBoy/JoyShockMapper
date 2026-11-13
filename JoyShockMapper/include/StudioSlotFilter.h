#pragma once
#include <cstdint>
#include <atomic>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

// A slot hotkey's complete controller input set is consumed before bindings
// run. Partial sets are untouched, including indefinitely held chord roots.
namespace StudioSlotFilter {
struct Rule { int owner; uint64_t buttons; unsigned mouse; };
inline std::mutex mutex;
inline std::vector<Rule> rules;
inline std::atomic<bool> configured{false};
inline bool configure(const std::string &text) {
    std::vector<Rule> next;
    std::istringstream stream(text);
    std::string part;
    while (std::getline(stream, part, ';')) {
        if (part.empty()) continue;
        if (part.find_first_not_of("0123456789 \t") != std::string::npos) return false;
        Rule rule{}; std::istringstream input(part); std::string extra;
        if (!(input >> rule.owner >> rule.buttons >> rule.mouse) || (input >> extra) || rule.owner <= 0 || !rule.buttons || rule.mouse > 3 || next.size() >= 64) return false;
        next.push_back(rule);
    }
    std::lock_guard<std::mutex> guard(mutex); rules = std::move(next); configured.store(!rules.empty(),std::memory_order_relaxed); return true;
}
inline uint64_t filter(int owner, uint64_t buttons, unsigned mouse) {
    if (!configured.load(std::memory_order_relaxed)) return buttons;
    uint64_t consumed = 0;
    std::lock_guard<std::mutex> guard(mutex);
    for (const auto &rule : rules)
        if (rule.owner == owner && (buttons & rule.buttons) == rule.buttons && (mouse & rule.mouse) == rule.mouse) consumed |= rule.buttons;
    return buttons & ~consumed;
}
}
