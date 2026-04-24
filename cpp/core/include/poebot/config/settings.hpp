#pragma once
#include <poebot/config/profile.hpp>
#include <string>
#include <unordered_map>

namespace poebot::config {

// Top-level persisted application state. All user-adjustable values live here.
struct Settings {
    int version = 1;                      // schema version; bump on breaking changes
    std::string language = "zh";          // "zh" | "en"
    std::string appearance = "light";     // "light" | "dark" (macOS-style theme)
    std::string activeProfile = "poe1";   // key into `profiles`
    std::unordered_map<std::string, GameProfile> profiles;

    // Returns pointer to the currently active profile, or nullptr if missing.
    GameProfile* active() noexcept;
    const GameProfile* active() const noexcept;

    // Build a Settings object with built-in poe1 + poe2 profiles.
    static Settings makeDefault();
};

}  // namespace poebot::config
