#pragma once
#include <poebot/gui/hotkey_capture.hpp>
#include <poebot/gui/panel.hpp>
#include <poebot/i18n/i18n.hpp>

#include <string>

namespace poebot::gui::panels {

// Settings panel — owns Hotkeys (top) and per-profile coord config (below).
// `name()` stays "Config" (stable internal id used elsewhere as activePanel
// key); the user-facing label flips to "Settings"/"设置" via the i18n table.
class ConfigPanel : public Panel {
public:
    const char* name()  const override { return "Config"; }
    const char* label() const override { return poebot::i18n::tr("panel.config"); }
    void        render(PanelContext& ctx) override;

private:
    // Rebind modal state. While `rebindingId_` is non-empty the modal is
    // open and the hook in `capture_` is intercepting all keyboard input
    // process-wide. ESC or a successful commit clears this back to "".
    std::string                           rebindingId_;
    std::string                           rebindError_;
    poebot::gui::HotkeyCapture            capture_;
};

}  // namespace poebot::gui::panels
