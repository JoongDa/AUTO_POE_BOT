#pragma once
#include <filesystem>
#include <string>

// UI for binding the active panel (Craft or Map) to one of the on-disk
// affix libraries. Each library is one .txt file under `dir`; rules are
// `|`-separated regex (one per line is conventional). Editing happens in
// the user's external editor — the widget never owns a textbox.
//
// Typical usage:
//   affixLibraryWidget(
//       ctx.affixLibraryDir ? *ctx.affixLibraryDir / "craft" : std::filesystem::path{},
//       prof->craft.affixLibrary,
//       prof->craft.affixes,
//       "craft",     // id scope — keeps ImGui widget ids unique per panel
//       &changed);
//   if (changed) ctx.dirty = true;
//
// The widget renders:
//   - A Combo of library names (current selection comes from `selected`).
//   - Edit / Reload / Folder buttons:
//       * Edit   → ShellExecute the bound .txt with the OS default editor.
//       * Reload → force re-read the bound file (manual fallback if the
//                  file watcher missed an mtime change).
//       * Folder → open the dir in Explorer; new/rename/delete go through
//                  the file system (no in-app file manager).
//   - An inline "(N rules)" status next to the combo.
//   - A clickable "Supports https://poe.re/" hint.
//   - An always-on file watcher that auto-reloads `content` when the bound
//     .txt changes mtime — round-trip after the user saves in their editor
//     is just "switch back to GUI", no Reload click needed.
namespace poebot::gui {

// Returns true if `selected` or `content` was modified this frame (caller
// should set ctx.dirty). `content` may be mutated by:
//   - the user picking a library from the dropdown,
//   - the file watcher noticing the bound file changed on disk,
//   - the user clicking Reload.
// Pass an empty `dir` to render the widget in a disabled state.
void affixLibraryWidget(const std::filesystem::path& dir,
                        std::string& selected,
                        std::string& content,
                        const char* idScope,
                        bool* outChanged);

}  // namespace poebot::gui
