#pragma once
#include <poebot/coords.hpp>
#include <string>
#include <string_view>

namespace poebot::config {

// All coordinates for one profile, in game client space.
// Meanings mirror the original AHK variables to ease porting.
struct ProfileCoords {
    ClientPoint orb1{};       // currency 1 stack (left click = pick up)
    ClientPoint orb2{};       // currency 2 stack
    ClientPoint orb3{};       // currency 3 stack
    ClientPoint baseItem{};   // reference item at grid (row=0, col=0)
    ClientPoint p01Item{};    // item at (row=0, col=1) — used to derive X offset
    ClientPoint p10Item{};    // item at (row=1, col=0) — used to derive Y offset
    ClientPoint invBase{};    // inventory cell (0,0) for deposit
    ClientPoint invP01{};     // inventory (0,1) — X offset
    ClientPoint invP10{};     // inventory (1,0) — Y offset
};

struct CraftSettings {
    bool batch = false;
    int cols = 10;
    int rows = 2;
    std::string affixes;  // regex alternatives, '|' separated
};

enum class MapMode {
    AlchAndScour = 1,  // 点金 + 重铸
    Chaos = 2,         // 混沌石
};

struct MapSettings {
    MapMode mode = MapMode::AlchAndScour;
    bool batch = false;
    int cols = 4;
    int rows = 3;
    std::string affixes;
};

struct DepositSettings {
    int cols = 12;
    int rows = 5;
};

struct ProfileStats {
    int craftOps = 0;
    int craftHits = 0;
    int mapOps = 0;
    int mapHits = 0;
};

// Full game profile: game identification + all coords + task settings.
struct GameProfile {
    std::string name;                   // profile key, e.g. "poe1"
    std::string displayName;            // UI-facing, e.g. "Path of Exile 1"
    std::string windowTitlePattern;     // substring to match with WinGetTitle
    ProfileCoords coords{};
    CraftSettings craft{};
    MapSettings map{};
    DepositSettings deposit{};
    ProfileStats stats{};
};

// Factory: return the default profile for a given version key ("poe1" / "poe2").
GameProfile defaultProfileFor(std::string_view version);

}  // namespace poebot::config
