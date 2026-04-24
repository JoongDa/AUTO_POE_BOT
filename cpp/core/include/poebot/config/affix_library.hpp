#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Affix-library storage. Each library is one UTF-8 text file under `dir`
// (e.g. `<exe>/affix_libraries/my-endgame.txt`). Libraries are independent
// from the active profile: the profile holds whatever the user currently
// has in the affix textbox, and libraries are separate reusable snippets
// that can be loaded into or saved from that textbox.
//
// All functions are thread-compatible but not thread-safe: callers serialize
// on the UI thread (file ops happen in response to user actions and are
// cheap enough that async isn't warranted).
namespace poebot::config {

// Create `dir` if it doesn't exist. Returns false if creation failed AND
// the directory still doesn't exist. Safe to call repeatedly.
bool ensureAffixLibraryDir(const std::filesystem::path& dir);

// Return every *.txt file under `dir`, as library names (stem only, no
// extension, no directory). Sorted case-insensitively. Missing directory
// returns an empty list (callers shouldn't need to pre-check).
std::vector<std::string> listAffixLibraries(const std::filesystem::path& dir);

// Read one library's full content. Returns empty string if the file is
// missing OR unreadable — the UI treats "missing" and "empty" identically.
std::string loadAffixLibrary(const std::filesystem::path& dir,
                             std::string_view name);

// Write `content` to `<dir>/<name>.txt` atomically (temp file + rename),
// matching the settings_io approach so a crash mid-write doesn't truncate
// the existing file. `name` must already be sanitized via isValidAffixLibraryName.
bool saveAffixLibrary(const std::filesystem::path& dir,
                      std::string_view name,
                      std::string_view content);

// Rename one library. Fails if the source doesn't exist, the target
// already exists, or the target name is invalid. No fallback / merge —
// the UI confirms with the user before calling this.
bool renameAffixLibrary(const std::filesystem::path& dir,
                        std::string_view from,
                        std::string_view to);

// Remove one library file. Returns true if the file was removed OR
// already absent (idempotent). Returns false only on filesystem errors.
bool deleteAffixLibrary(const std::filesystem::path& dir,
                        std::string_view name);

// Reject names that would collide with the filesystem or produce files the
// OS can't handle: empty, any of `\/:*?"<>|`, trailing dot/space, or > 64
// chars. Keeps names short enough to fit the dropdown comfortably.
bool isValidAffixLibraryName(std::string_view name);

}  // namespace poebot::config
