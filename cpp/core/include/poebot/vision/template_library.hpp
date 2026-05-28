#pragma once
#include <poebot/vision/template_match.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace poebot::vision {

// One template loaded from disk. The basename of the PNG file (without
// extension) becomes the coord name and is also the key under which any
// matches end up in the profile's calibrated map.
struct TemplateEntry {
    std::string name;           // PNG basename, e.g. "chaos", "scour", "orb_currency"
    ImageBGRA   image;          // BGRA pixels at the file's original resolution
    bool        loaded = false; // false if the file failed to decode
};

// Scans a directory for arbitrary PNG files. Each `*.png` becomes a coord
// candidate — there is no hardcoded list. The basename (without extension)
// is the canonical name used everywhere downstream:
//   - the row label in the Auto Calibrate UI
//   - the key in GameProfile::calibrated
//   - the suffix root when one template matches multiple instances
//     ("chaos_1", "chaos_2", …)
//
// Failed-to-decode files are kept in entries() with loaded=false so the
// UI can still warn about them.
class TemplateLibrary {
public:
    // (Re)load from `templatesDir`. Clears previous state first. Returns
    // true when at least one PNG decoded successfully.
    bool load(const std::filesystem::path& templatesDir);

    bool reload() { return load(dir_); }

    const std::filesystem::path&      dir()     const noexcept { return dir_;     }
    const std::vector<TemplateEntry>& entries() const noexcept { return entries_; }
    bool empty()     const noexcept { return entries_.empty();  }
    bool anyLoaded() const noexcept;

    // Replace (or create) <name>.png inside the current templates directory.
    // The image must be BGRA (same layout produced by load()). Writes
    // atomically via a temp-file rename, then calls reload() to refresh the
    // in-memory entry list. Returns false on encode or I/O failure.
    bool replaceTemplate(const std::string& name, const ImageBGRA& image);

private:
    std::filesystem::path      dir_;
    std::vector<TemplateEntry> entries_;
};

}  // namespace poebot::vision
