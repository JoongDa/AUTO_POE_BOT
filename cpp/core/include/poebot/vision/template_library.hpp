#pragma once
#include <poebot/vision/template_match.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace poebot::vision {

// One coordinate template loaded from disk.
struct TemplateEntry {
    std::string coordName;        // "orb1", "baseItem", etc.
    ImageBGRA   image;            // BGRA pixels at original (file) resolution
    bool        loaded = false;   // false when PNG was missing or unreadable
};

// Scans a directory for PNG files whose basenames match the nine coordinate
// fields understood by findCoordByName():
//   orb1.png, orb2.png, orb3.png,
//   baseItem.png, p01Item.png, p10Item.png,
//   invBase.png, invP01.png, invP10.png
//
// Missing files yield entries with loaded=false so the UI always shows a row
// for every field regardless of which templates are present on disk.
class TemplateLibrary {
public:
    // Display order. File names in the templates directory are derived from
    // these by appending ".png".
    static const std::vector<std::string>& coordNames();

    // (Re)load from `templatesDir`. Clears any previous state first. Returns
    // true when at least one PNG loaded successfully.
    bool load(const std::filesystem::path& templatesDir);

    // Reload from the directory last passed to load().
    bool reload() { return load(dir_); }

    const std::filesystem::path&      dir()     const noexcept { return dir_;     }
    const std::vector<TemplateEntry>& entries() const noexcept { return entries_; }
    bool empty()     const noexcept { return entries_.empty();  }
    bool anyLoaded() const noexcept;

private:
    std::filesystem::path      dir_;
    std::vector<TemplateEntry> entries_;
};

}  // namespace poebot::vision
