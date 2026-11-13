#pragma once
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <utility>
#include <stdexcept>

// An absent file preserves standalone JSM compatibility. Once present, only
// explicitly listed hardware models are managed; empty/invalid files fail closed.
class ManagedControllers {
    bool restricted = false;
    std::set<std::pair<unsigned, unsigned>> devices;
public:
    static ManagedControllers parse(std::istream &input) {
        ManagedControllers result;
        result.restricted = true;
        std::string line;
        while (std::getline(input, line)) {
            line = line.substr(0, line.find('#'));
            std::istringstream words(line);
            std::string id, extra;
            if (!(words >> id)) continue;
            if (words >> extra || id.size() != 9 || id[4] != ':')
                throw std::runtime_error("Expected a VID:PID pair of four hexadecimal digits each");
            for (size_t i = 0; i < id.size(); ++i)
                if (i != 4 && !((id[i] >= '0' && id[i] <= '9') ||
                    (id[i] >= 'a' && id[i] <= 'f') || (id[i] >= 'A' && id[i] <= 'F')))
                    throw std::runtime_error("Invalid hexadecimal controller ID");
            result.devices.emplace(std::stoul(id.substr(0, 4), nullptr, 16),
                std::stoul(id.substr(5), nullptr, 16));
        }
        return result;
    }
    static ManagedControllers load(const std::string &path) {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Cannot read managed controller policy");
        return parse(input);
    }
    static ManagedControllers denyAll() { ManagedControllers p; p.restricted = true; return p; }
    bool enabled() const { return restricted; }
    bool allows(unsigned vendor, unsigned product) const {
        return !restricted || devices.count({vendor, product}) != 0;
    }
};
