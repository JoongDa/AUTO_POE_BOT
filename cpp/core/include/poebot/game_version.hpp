#pragma once
#include <string_view>

namespace poebot {

enum class GameVersion {
    POE1,
    POE2,
    Unknown,
};

constexpr std::string_view toString(GameVersion v) noexcept {
    switch (v) {
        case GameVersion::POE1: return "poe1";
        case GameVersion::POE2: return "poe2";
        default:                return "unknown";
    }
}

constexpr GameVersion parseGameVersion(std::string_view s) noexcept {
    if (s == "poe1") return GameVersion::POE1;
    if (s == "poe2") return GameVersion::POE2;
    return GameVersion::Unknown;
}

constexpr std::string_view displayName(GameVersion v) noexcept {
    switch (v) {
        case GameVersion::POE1: return "Path of Exile 1";
        case GameVersion::POE2: return "Path of Exile 2";
        default:                return "Unknown";
    }
}

}  // namespace poebot
