#pragma once
#include <poebot/config/settings.hpp>
#include <filesystem>

namespace poebot::config {

// --- Split-layout API --------------------------------------------------------
// The runtime layout keeps per-game data in its own directory so poe1 and
// poe2 don't share coords, craft patterns, stats, or affix libraries:
//
//   <root>/app.json               — top-level meta (activeProfile, language,
//                                   appearance, schema version)
//   <root>/<name>/settings.json   — one GameProfile per directory
//   <root>/<name>/affix_libraries/{craft,map}/*.txt
//
// `loadLayout` is the canonical entry point: it runs one-time migration from
// any legacy <root>/settings.json + <root>/affix_libraries/ flat layout,
// then reads the new files. `saveLayout` writes both app.json and each
// profile atomically (tmp + rename per file).

Settings loadLayout(const std::filesystem::path& root);
bool     saveLayout(const Settings& s, const std::filesystem::path& root);

// Absolute path to a profile's affix_libraries root. Panels append the
// "craft" or "map" subfolder.
std::filesystem::path affixLibraryDirFor(const std::filesystem::path& root,
                                         const std::string& profileName);

}  // namespace poebot::config
