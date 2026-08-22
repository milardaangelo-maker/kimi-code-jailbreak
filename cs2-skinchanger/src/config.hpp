#pragma once

// Minimal "key = value" config reader. '#' starts a comment, whitespace is
// trimmed, values may be decimal, hex (0x...), floats, or plain strings.
// Offsets live in config files so a CS2 update only requires regenerating
// offsets.cfg (tools/cs2dumper_to_cfg.py), never a rebuild.

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

class Config {
public:
    bool Load(const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open())
            return false;

        std::string line;
        while (std::getline(in, line)) {
            const auto hash = line.find('#');
            if (hash != std::string::npos)
                line.erase(hash);

            const auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = Trim(line.substr(0, eq));
            std::string value = Trim(line.substr(eq + 1));
            if (!key.empty())
                values_[key] = value;
        }
        return true;
    }

    bool Has(const std::string& key) const { return values_.count(key) != 0; }

    std::string GetString(const std::string& key, const std::string& fallback = "") const {
        auto it = values_.find(key);
        return it == values_.end() ? fallback : it->second;
    }

    uint64_t GetU64(const std::string& key, uint64_t fallback = 0) const {
        auto it = values_.find(key);
        if (it == values_.end())
            return fallback;
        try {
            return std::stoull(it->second, nullptr, 0); // base 0: handles 0x.. and decimal
        } catch (...) {
            return fallback;
        }
    }

    int GetInt(const std::string& key, int fallback = 0) const {
        return static_cast<int>(GetU64(key, static_cast<uint64_t>(fallback)));
    }

    float GetFloat(const std::string& key, float fallback = 0.f) const {
        auto it = values_.find(key);
        if (it == values_.end())
            return fallback;
        try {
            return std::stof(it->second);
        } catch (...) {
            return fallback;
        }
    }

    bool GetBool(const std::string& key, bool fallback = false) const {
        auto it = values_.find(key);
        if (it == values_.end())
            return fallback;
        return it->second == "1" || it->second == "true" || it->second == "yes";
    }

    const std::unordered_map<std::string, std::string>& All() const { return values_; }

private:
    static std::string Trim(const std::string& s) {
        const auto b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos)
            return "";
        const auto e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
    }

    std::unordered_map<std::string, std::string> values_;
};
