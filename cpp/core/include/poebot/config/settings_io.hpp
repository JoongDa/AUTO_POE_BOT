#pragma once
#include <poebot/config/settings.hpp>
#include <filesystem>

namespace poebot::config {

// Load Settings from JSON file. If the file doesn't exist or is malformed,
// returns Settings::makeDefault() and logs a warning via spdlog.
Settings loadSettings(const std::filesystem::path& path);

// Save Settings to JSON. Uses tempfile + atomic rename so a crash mid-write
// cannot leave the real file truncated.
// Returns true on success; logs errors via spdlog.
bool saveSettings(const Settings& s, const std::filesystem::path& path);

}  // namespace poebot::config
