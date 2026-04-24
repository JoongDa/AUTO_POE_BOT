#pragma once
#include <filesystem>
#include <string>

// UI for browsing, loading, and editing affix libraries. Shared between
// the Craft and Map panels — they pass their own bound strings so the
// widget reads/writes the right profile field without knowing which one.
//
// Typical usage:
//   affixLibraryWidget(
//       ctx.affixLibraryDir ? *ctx.affixLibraryDir : std::filesystem::path{},
//       prof->craft.affixLibrary,
//       prof->craft.affixes,
//       "craft",     // id scope — keeps ImGui widget ids unique per panel
//       &changed);
//   if (changed) ctx.dirty = true;
//
// The widget renders:
//   - A Combo of library names (current selection comes from `selected`).
//   - New / Save / Rename / Delete buttons with inline modal prompts.
//   - A subtle "unsaved" indicator when `content` != on-disk library.
//   - A clickable "Supports https://poe.re/" hint.
//
// It does NOT render the affix textbox itself — that stays in each panel
// because it uses panel-specific width / label / imgui id already.
namespace poebot::gui {

// Returns true if `selected` or `content` was modified this frame (caller
// should set ctx.dirty). `content` may be mutated when the user picks a
// library from the dropdown; `selected` may be mutated by any library
// action. Pass an empty `dir` to render the widget in a disabled state.
void affixLibraryWidget(const std::filesystem::path& dir,
                        std::string& selected,
                        std::string& content,
                        const char* idScope,
                        bool* outChanged);

}  // namespace poebot::gui
