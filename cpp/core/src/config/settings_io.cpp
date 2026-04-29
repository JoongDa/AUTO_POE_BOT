#include <poebot/config/settings_io.hpp>

#include <poebot/hotkey/binding.hpp>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
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

namespace {

int readGridDimension(const json& j,
                      const char* section,
                      const char* key,
                      int fallback,
                      int minValue,
                      int maxValue) {
    const auto it = j.find(key);
    if (it == j.end()) return fallback;

    if (!it->is_number_integer()) {
        spdlog::warn("settings: {}.{} must be an integer in [{}..{}], using default {}",
                     section, key, minValue, maxValue, fallback);
        return fallback;
    }

    if (it->is_number_unsigned()) {
        const auto raw = it->get<std::uint64_t>();
        if (raw < static_cast<std::uint64_t>(minValue) ||
            raw > static_cast<std::uint64_t>(maxValue)) {
            spdlog::warn("settings: {}.{}={} outside allowed range [{}..{}], using default {}",
                         section, key, raw, minValue, maxValue, fallback);
            return fallback;
        }
        return static_cast<int>(raw);
    }

    const auto raw = it->get<std::int64_t>();
    if (raw < minValue || raw > maxValue) {
        spdlog::warn("settings: {}.{}={} outside allowed range [{}..{}], using default {}",
                     section, key, raw, minValue, maxValue, fallback);
        return fallback;
    }
    return static_cast<int>(raw);
}

}  // namespace

inline void to_json(json& j, const ProfileCoords& c) {
    j = json{
        {"orb1", c.orb1},
        {"orb2", c.orb2},
        {"orb3", c.orb3},
        {"orb1Qty", c.orb1Qty},
        {"orb2Qty", c.orb2Qty},
        {"orb3Qty", c.orb3Qty},
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
    c.orb1Qty = j.value("orb1Qty", -1);
    c.orb2Qty = j.value("orb2Qty", -1);
    c.orb3Qty = j.value("orb3Qty", -1);
    c.baseItem = j.value("baseItem", ClientPoint{});
    c.p01Item = j.value("p01Item", ClientPoint{});
    c.p10Item = j.value("p10Item", ClientPoint{});
    c.invBase = j.value("invBase", ClientPoint{});
    c.invP01 = j.value("invP01", ClientPoint{});
    c.invP10 = j.value("invP10", ClientPoint{});
}

inline void to_json(json& j, const CraftSettings& c) {
    j = json{
        {"batch", c.batch},
        {"cols", c.cols},
        {"rows", c.rows},
        {"affixes", c.affixes},
        {"affixLibrary", c.affixLibrary},
    };
}
inline void from_json(const json& j, CraftSettings& c) {
    c.batch = j.value("batch", false);
    c.cols = readGridDimension(j, "craft", "cols", 10, 1, 24);
    c.rows = readGridDimension(j, "craft", "rows", 2, 1, 12);
    c.affixes = j.value("affixes", std::string{});
    c.affixLibrary = j.value("affixLibrary", std::string{});
}

inline void to_json(json& j, const MapSettings& m) {
    j = json{
        {"mode", static_cast<int>(m.mode)},
        {"batch", m.batch},
        {"cols", m.cols},
        {"rows", m.rows},
        {"affixes", m.affixes},
        {"affixLibrary", m.affixLibrary},
    };
}
inline void from_json(const json& j, MapSettings& m) {
    const int mode = j.value("mode", 1);
    m.mode = (mode == 2) ? MapMode::Chaos : MapMode::AlchAndScour;
    m.batch = j.value("batch", false);
    m.cols = readGridDimension(j, "map", "cols", 4, 1, 12);
    m.rows = readGridDimension(j, "map", "rows", 3, 1, 12);
    m.affixes = j.value("affixes", std::string{});
    m.affixLibrary = j.value("affixLibrary", std::string{});
}

inline void to_json(json& j, const DepositSettings& d) {
    j = json{{"cols", d.cols}, {"rows", d.rows}};
}
inline void from_json(const json& j, DepositSettings& d) {
    d.cols = readGridDimension(j, "deposit", "cols", 12, 1, 24);
    d.rows = readGridDimension(j, "deposit", "rows", 5, 1, 12);
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
        {"appearance", s.appearance},
        {"activeProfile", s.activeProfile},
        {"profiles", s.profiles},
    };
}
inline void from_json(const json& j, Settings& s) {
    s.version = j.value("version", 1);
    s.language = j.value("language", std::string{"zh"});
    s.appearance = j.value("appearance", std::string{"light"});
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

// --- Split-layout implementation --------------------------------------------

namespace {

// Known profile keys — each gets its own directory under root. Keeping this
// static avoids the GUI inventing arbitrary keys that would pollute disk.
constexpr std::array<const char*, 2> kProfileKeys = {"poe1", "poe2"};

std::filesystem::path profileDir(const std::filesystem::path& root, const std::string& name) {
    return root / name;
}
std::filesystem::path profileFile(const std::filesystem::path& root, const std::string& name) {
    return root / name / "settings.json";
}

// Atomic write: tmp + rename. Mirrors saveSettings() so callers get the same
// crash-safety guarantees.
bool atomicWriteJson(const std::filesystem::path& path, const json& j) {
    try {
        if (auto dir = path.parent_path(); !dir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                spdlog::error("mkdir {}: {}", dir.string(), ec.message());
                return false;
            }
        }
        std::filesystem::path tmp = path;
        tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) { spdlog::error("open tmp {}: failed", tmp.string()); return false; }
            f << j.dump(2);
            if (!f) { spdlog::error("write tmp {}: failed", tmp.string()); return false; }
        }
        std::error_code ec;
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            spdlog::error("rename {} -> {}: {}", tmp.string(), path.string(), ec.message());
            std::error_code rm_ec;
            std::filesystem::remove(tmp, rm_ec);
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("atomicWriteJson exception: {}", e.what());
        return false;
    }
}

// Top-level meta file: activeProfile + language + appearance + global hotkey
// bindings (everything that isn't per-game). Keeps the swap-active-profile
// action snappy (one small file rewrite) even if each profile's settings.json
// grows.
struct AppMeta {
    int version = 1;
    std::string language = "zh";
    std::string appearance = "light";
    std::string activeProfile = "poe1";
    poebot::hotkey::HotkeyConfig hotkeys = poebot::hotkey::defaultHotkeys();
};

// Hotkeys serialize as a string-keyed object of human-readable bindings:
//   "hotkeys": { "task.stop": "End", "capture.orb1": "Alt+1", ... }
// Unknown keys (e.g. an action removed in a future version) are kept
// verbatim on round-trip but ignored at use time. Missing keys are filled
// from defaults during from_json so older files keep working without an
// explicit migration step.
json hotkeysToJson(const poebot::hotkey::HotkeyConfig& h) {
    json j = json::object();
    for (const auto& [id, b] : h) j[id] = b.format();
    return j;
}
poebot::hotkey::HotkeyConfig hotkeysFromJson(const json& j) {
    auto cfg = poebot::hotkey::defaultHotkeys();
    if (!j.is_object()) return cfg;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (!it.value().is_string()) continue;
        auto parsed = poebot::hotkey::HotkeyBinding::parse(it.value().get<std::string>());
        if (parsed.valid()) cfg[it.key()] = parsed;
        // Unparseable entries fall through to whatever was in defaults.
    }
    return cfg;
}

void to_json(json& j, const AppMeta& m) {
    j = json{
        {"version", m.version},
        {"language", m.language},
        {"appearance", m.appearance},
        {"activeProfile", m.activeProfile},
        {"hotkeys", hotkeysToJson(m.hotkeys)},
    };
}
void from_json(const json& j, AppMeta& m) {
    m.version       = j.value("version", 1);
    m.language      = j.value("language", std::string{"zh"});
    m.appearance    = j.value("appearance", std::string{"light"});
    m.activeProfile = j.value("activeProfile", std::string{"poe1"});
    if (j.contains("hotkeys")) {
        m.hotkeys = hotkeysFromJson(j["hotkeys"]);
    } else {
        m.hotkeys = poebot::hotkey::defaultHotkeys();
    }
}

bool readAppMeta(const std::filesystem::path& path, AppMeta& out) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;
    try {
        std::ifstream f(path);
        if (!f) return false;
        json j; f >> j;
        out = j.get<AppMeta>();
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("app.json parse failed ({}): {}", path.string(), e.what());
        return false;
    }
}

bool readProfile(const std::filesystem::path& path, GameProfile& out) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;
    try {
        std::ifstream f(path);
        if (!f) return false;
        json j; f >> j;
        out = j.get<GameProfile>();
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("profile parse failed ({}): {}", path.string(), e.what());
        return false;
    }
}

// Recursively copies a directory tree. Used during the one-shot migration
// from the flat legacy affix_libraries/ layout.
void copyDirIfPresent(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::error_code ec;
    if (!std::filesystem::exists(from, ec) || !std::filesystem::is_directory(from, ec)) return;
    std::filesystem::create_directories(to, ec);
    std::filesystem::copy(from, to,
                          std::filesystem::copy_options::recursive |
                          std::filesystem::copy_options::skip_existing, ec);
    if (ec) {
        spdlog::warn("migrate copy {} -> {}: {}", from.string(), to.string(), ec.message());
    }
}

// One-shot migration from the flat <root>/settings.json + <root>/affix_libraries
// layout to the per-profile tree. Runs only if app.json is missing AND the
// legacy settings.json exists — otherwise a fresh install or already-migrated
// install no-ops safely.
void migrateLegacyIfNeeded(const std::filesystem::path& root) {
    const auto appJson    = root / "app.json";
    const auto legacy     = root / "settings.json";
    const auto legacyLibs = root / "affix_libraries";

    std::error_code ec;
    if (std::filesystem::exists(appJson, ec)) return;       // new layout already in place
    if (!std::filesystem::exists(legacy, ec)) return;       // nothing to migrate

    spdlog::info("settings: migrating legacy layout at {}", root.string());

    Settings legacyS = loadSettings(legacy);  // uses the existing whole-file reader

    AppMeta meta;
    meta.version       = legacyS.version;
    meta.language      = legacyS.language;
    meta.appearance    = legacyS.appearance;
    meta.activeProfile = legacyS.activeProfile;
    // Legacy settings predate user-customizable hotkeys, so seed defaults.
    meta.hotkeys       = poebot::hotkey::defaultHotkeys();
    atomicWriteJson(appJson, meta);

    // Write each profile to its own dir.
    for (const auto& [name, prof] : legacyS.profiles) {
        atomicWriteJson(profileFile(root, name), prof);
    }

    // Move legacy libraries into the previously active profile's dir. Keeping
    // them only on one side avoids duplicate templates; the user can re-save
    // in the other profile if they want copies there.
    if (!meta.activeProfile.empty()) {
        copyDirIfPresent(legacyLibs, root / meta.activeProfile / "affix_libraries");
    }

    // Rename the legacy file so next launch doesn't re-migrate. We keep it
    // around as a .bak rather than delete, just in case the user wants to
    // inspect it.
    const auto legacyBak = root / "settings.json.bak";
    std::filesystem::rename(legacy, legacyBak, ec);
    if (ec) {
        spdlog::warn("migrate: could not rename legacy settings.json: {}", ec.message());
    }
}

}  // namespace

Settings loadLayout(const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::create_directories(root, ec);

    migrateLegacyIfNeeded(root);

    Settings s = Settings::makeDefault();  // baseline, will be overwritten below

    AppMeta meta;
    if (readAppMeta(root / "app.json", meta)) {
        s.version       = meta.version;
        s.language      = meta.language;
        s.appearance    = meta.appearance;
        s.activeProfile = meta.activeProfile;
        s.hotkeys       = std::move(meta.hotkeys);
    } else {
        spdlog::info("settings: no app.json at {}, using defaults", root.string());
    }

    // Always materialize both known profiles so the menu can switch between
    // them even if only one has been saved so far. Per-profile file missing
    // or corrupt falls back to defaultProfileFor.
    s.profiles.clear();
    for (const char* key : kProfileKeys) {
        GameProfile prof = defaultProfileFor(key);
        GameProfile loaded;
        if (readProfile(profileFile(root, key), loaded)) {
            prof = std::move(loaded);
            if (prof.name.empty()) prof.name = key;  // tolerate older files without `name`
        }
        s.profiles.emplace(key, std::move(prof));
    }

    // activeProfile must point at a profile we actually have; fall back to
    // poe1 if someone hand-edited the meta to a stale key.
    if (!s.profiles.count(s.activeProfile)) {
        s.activeProfile = "poe1";
    }
    return s;
}

bool saveLayout(const Settings& s, const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::create_directories(root, ec);

    AppMeta meta;
    meta.version       = s.version;
    meta.language      = s.language;
    meta.appearance    = s.appearance;
    meta.activeProfile = s.activeProfile;
    meta.hotkeys       = s.hotkeys;

    bool ok = atomicWriteJson(root / "app.json", meta);
    for (const auto& [name, prof] : s.profiles) {
        // Only write known profile keys to disk — avoids accidental folder
        // creation if something pushed a bogus key into the map.
        const bool known = std::any_of(kProfileKeys.begin(), kProfileKeys.end(),
                                       [&](const char* k) { return name == k; });
        if (!known) continue;
        ok = atomicWriteJson(profileFile(root, name), prof) && ok;
    }
    return ok;
}

std::filesystem::path affixLibraryDirFor(const std::filesystem::path& root,
                                         const std::string& profileName) {
    return root / profileName / "affix_libraries";
}

}  // namespace poebot::config
