#pragma once
#include <poebot/config/settings.hpp>
#include <poebot/hotkey/binding.hpp>
#include <poebot/log/log_sink.hpp>

#include <filesystem>
#include <functional>
#include <string>

namespace poebot::win  { class GameWindow; }
namespace poebot::task { class TaskRunner; }

namespace poebot::gui {

class CaptureService;

// Distinguishes panels that live in the top tab bar from the log strip along
// the bottom. Phase 3 may add more layout zones.
enum class PanelKind {
    Tab,
    Log,
};

// Shared state panels can read/write during render().
struct PanelContext {
    poebot::config::Settings* settings     = nullptr;
    poebot::log::ImGuiSink*   logSink      = nullptr;
    // Root directory of the split-layout tree (contains app.json and each
    // profile's subfolder). Config panel uses this to persist via saveLayout.
    const std::filesystem::path* settingsRoot    = nullptr;
    // Affix library root for the CURRENTLY ACTIVE profile. App updates this
    // each frame so switching profiles immediately retargets the picker to
    // the new game's templates. Each panel appends its own category
    // ("craft" or "map") as a subfolder.
    const std::filesystem::path* affixLibraryDir  = nullptr;

    // Phase 3.1 — window + coord capture
    const poebot::win::GameWindow* gameWindow = nullptr;
    CaptureService*                capture    = nullptr;

    // Phase 3.2 — task engine
    poebot::task::TaskRunner*      taskRunner = nullptr;

    // Name of the currently selected side-bar panel (matches Panel::name()).
    // main_layout writes this; individual panels read it via ctx only when
    // they need to render something extra based on active state.
    std::string activePanel = "Config";

    // Panels set `dirty = true` when they have committed an edit to settings.
    // App::run() observes this and persists to disk (throttled).
    bool dirty = false;

    // Invoked when the user picks a different appearance (Light/Dark) from
    // the menu. App wires this to its applyAppearance() so palette, clear
    // color, and title bar repaint immediately.
    std::function<void()> onAppearanceChanged;

    // Live view of the hotkey map (App owns the storage). Settings panel
    // reads this every frame to render the current binding next to each
    // row. Pointer is non-null once App has finished wiring panelCtx_.
    const poebot::hotkey::HotkeyConfig* hotkeys = nullptr;

    // Bridge to App::rebindHotkey. Returns true on success, false on any
    // conflict / system-wide registration failure. The caller renders an
    // inline error in the rebind modal when this returns false; settings
    // are unchanged in that case.
    std::function<bool(const std::string& id,
                       poebot::hotkey::HotkeyBinding newBinding)> onRebindHotkey;
};

// Resolve the live binding for `actionId` and return its format() string
// ("Alt+1", "End", "Ctrl+Shift+D", …). Falls back to the action's default
// when the live map doesn't contain the id (e.g. on a fresh profile that
// just finished defaultHotkeys()-backfilling).
//
// Inline-defined here so every panel that wants to render a hotkey hint
// or a binding label gets the same lookup without each file open-coding
// its own copy.
inline std::string hotkeyLabel(const PanelContext& ctx, const char* actionId) {
    if (ctx.hotkeys) {
        if (auto it = ctx.hotkeys->find(actionId); it != ctx.hotkeys->end()) {
            return it->second.format();
        }
    }
    return poebot::hotkey::defaultBindingFor(actionId).format();
}

// Abstract base for every UI panel. Keep render() cheap; it runs every frame.
class Panel {
public:
    virtual ~Panel() = default;

    // Stable internal id — English, never changes, used as the key in settings
    // and in PanelContext::activePanel. Never shown to the user directly.
    virtual const char* name() const = 0;

    // Localized text shown in the sidebar / tab bar. Defaults to name() so
    // panels without a translation key still render something sensible.
    virtual const char* label() const { return name(); }

    virtual PanelKind   kind() const { return PanelKind::Tab; }

    virtual void render(PanelContext& ctx) = 0;
};

}  // namespace poebot::gui
