#include <poebot/config/settings_io.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <fstream>
#include <string>
#include <system_error>
#include <unordered_map>

using nlohmann::json;

// --- ADL-visible JSON conversions --------------------------------------------
// nlohmann::json finds to_json/from_json via ADL in the type's namespace.
// These definitions live only in this translation unit on purpose: the JSON
// schema is an implementation detail of settings_io and should not leak.

namespace poebot {

inline void to_json(json& j, const ClientPoint& p) {
    j = json{{"x", p.x}, {"y", p.y}};
}
inline void from_json(const json& j, ClientPoint& p) {
    p.x = j.value("x", 0);
    p.y = j.value("y", 0);
}

}  // namespace poebot

namespace poebot::config {

inline void to_json(json& j, const ProfileCoords& c) {
    j = json{
        {"orb1", c.orb1},
        {"orb2", c.orb2},
        {"orb3", c.orb3},
        {"baseItem", c.baseItem},
        {"p01Item", c.p01Item},
        {"p10Item", c.p10Item},
        {"invBase", c.invBase},
        {"invP01", c.invP01},
        {"invP10", c.invP10},
    };
}
inline void from_json(const json& j, ProfileCoords& c) {
    c.orb1 = j.value("orb1", ClientPoint{});
    c.orb2 = j.value("orb2", ClientPoint{});
    c.orb3 = j.value("orb3", ClientPoint{});
    c.baseItem = j.value("baseItem", ClientPoint{});
    c.p01Item = j.value("p01Item", ClientPoint{});
    c.p10Item = j.value("p10Item", ClientPoint{});
    c.invBase = j.value("invBase", ClientPoint{});
    c.invP01 = j.value("invP01", ClientPoint{});
    c.invP10 = j.value("invP10", ClientPoint{});
}

inline void to_json(json& j, const CraftSettings& c) {
    j = json{{"batch", c.batch}, {"cols", c.cols}, {"rows", c.rows}, {"affixes", c.affixes}};
}
inline void from_json(const json& j, CraftSettings& c) {
    c.batch = j.value("batch", false);
    c.cols = j.value("cols", 10);
    c.rows = j.value("rows", 2);
    c.affixes = j.value("affixes", std::string{});
}

inline void to_json(json& j, const MapSettings& m) {
    j = json{
        {"mode", static_cast<int>(m.mode)},
        {"batch", m.batch},
        {"cols", m.cols},
        {"rows", m.rows},
        {"affixes", m.affixes},
    };
}
inline void from_json(const json& j, MapSettings& m) {
    const int mode = j.value("mode", 1);
    m.mode = (mode == 2) ? MapMode::Chaos : MapMode::AlchAndScour;
    m.batch = j.value("batch", false);
    m.cols = j.value("cols", 4);
    m.rows = j.value("rows", 3);
    m.affixes = j.value("affixes", std::string{});
}

inline void to_json(json& j, const DepositSettings& d) {
    j = json{{"cols", d.cols}, {"rows", d.rows}};
}
inline void from_json(const json& j, DepositSettings& d) {
    d.cols = j.value("cols", 12);
    d.rows = j.value("rows", 5);
}

inline void to_json(json& j, const ProfileStats& s) {
    j = json{
        {"craftOps", s.craftOps},
        {"craftHits", s.craftHits},
        {"mapOps", s.mapOps},
        {"mapHits", s.mapHits},
    };
}
inline void from_json(const json& j, ProfileStats& s) {
    s.craftOps = j.value("craftOps", 0);
    s.craftHits = j.value("craftHits", 0);
    s.mapOps = j.value("mapOps", 0);
    s.mapHits = j.value("mapHits", 0);
}

inline void to_json(json& j, const GameProfile& p) {
    j = json{
        {"name", p.name},
        {"displayName", p.displayName},
        {"windowTitlePattern", p.windowTitlePattern},
        {"coords", p.coords},
        {"craft", p.craft},
        {"map", p.map},
        {"deposit", p.deposit},
        {"stats", p.stats},
    };
}
inline void from_json(const json& j, GameProfile& p) {
    p.name = j.value("name", std::string{});
    p.displayName = j.value("displayName", std::string{});
    p.windowTitlePattern = j.value("windowTitlePattern", std::string{});
    p.coords = j.value("coords", ProfileCoords{});
    p.craft = j.value("craft", CraftSettings{});
    p.map = j.value("map", MapSettings{});
    p.deposit = j.value("deposit", DepositSettings{});
    p.stats = j.value("stats", ProfileStats{});
}

inline void to_json(json& j, const Settings& s) {
    j = json{
        {"version", s.version},
        {"language", s.language},
        {"activeProfile", s.activeProfile},
        {"profiles", s.profiles},
    };
}
inline void from_json(const json& j, Settings& s) {
    s.version = j.value("version", 1);
    s.language = j.value("language", std::string{"zh"});
    s.activeProfile = j.value("activeProfile", std::string{"poe1"});
    s.profiles = j.value("profiles", std::unordered_map<std::string, GameProfile>{});
}

// --- Public API --------------------------------------------------------------

Settings loadSettings(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        spdlog::info("settings file not found, using defaults: {}", path.string());
        return Settings::makeDefault();
    }

    try {
        std::ifstream f(path);
        if (!f) {
            spdlog::warn("cannot open settings file, using defaults: {}", path.string());
            return Settings::makeDefault();
        }
        json j;
        f >> j;
        Settings s = j.get<Settings>();
        // If the file has no profiles (corrupted or truncated), fall back.
        if (s.profiles.empty()) {
            spdlog::warn("settings file has no profiles, using defaults: {}", path.string());
            return Settings::makeDefault();
        }
        return s;
    } catch (const std::exception& e) {
        spdlog::warn("settings parse failed ({}): {} — using defaults", path.string(), e.what());
        return Settings::makeDefault();
    }
}

bool saveSettings(const Settings& s, const std::filesystem::path& path) {
    try {
        if (auto dir = path.parent_path(); !dir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                spdlog::error("failed to create settings directory {}: {}", dir.string(), ec.message());
                return false;
            }
        }

        std::filesystem::path tmp = path;
        tmp += ".tmp";

        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) {
                spdlog::error("cannot open settings tmp file: {}", tmp.string());
                return false;
            }
            const json j = s;
            f << j.dump(2);
            if (!f) {
                spdlog::error("failed writing settings tmp file: {}", tmp.string());
                return false;
            }
        }  // ofstream closes here, flushing to disk

        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            spdlog::error("failed renaming {} -> {}: {}", tmp.string(), path.string(), ec.message());
            std::error_code rm_ec;
            std::filesystem::remove(tmp, rm_ec);  // best-effort cleanup
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("saveSettings exception: {}", e.what());
        return false;
    }
}

}  // namespace poebot::config
