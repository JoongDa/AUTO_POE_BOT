#pragma once
#include <poebot/coords.hpp>
#include <poebot/gui/hotkey_capture.hpp>
#include <poebot/gui/panel.hpp>
#include <poebot/i18n/i18n.hpp>

#include <string>

namespace poebot::gui::panels {

// Settings panel — Hotkeys tab (global hotkeys only) + Coordinates tab
// (per-profile coords with rebind buttons that target each coord's
// capture.* hotkey). `name()` stays "Config" (stable internal id used
// as activePanel key); the user-facing label flips to "Settings" /
// "设置" via the i18n table.
class ConfigPanel : public Panel {
public:
    const char* name()  const override { return "Config"; }
    const char* label() const override { return poebot::i18n::tr("panel.config"); }
    void        render(PanelContext& ctx) override;

private:
    // One coord row inside the Coordinates tab: name + value + a small
    // button labeled with the live binding for "capture.<name>". Clicking
    // the button opens the rebind modal — the actual capture flow runs
    // only when the user presses the bound hotkey in-game (Alt+1 by
    // default). Member function (not free) so the click handler can
    // poke rebindingId_ + capture_ without ConfigPanel having to leak
    // refs into a helper.
    void coordRow(PanelContext& ctx, const char* name, poebot::ClientPoint& p);

    // Common click handler for any binding button on this panel — sets
    // rebind state and arms the keyboard hook. Called from coordRow and
    // from the Hotkeys-tab inline loop.
    void requestRebind(const char* actionId);

    // Rebind modal state. While `rebindingId_` is non-empty the modal is
    // open and the hook in `capture_` is intercepting all keyboard input
    // process-wide. ESC or a successful commit clears this back to "".
    std::string                           rebindingId_;
    std::string                           rebindError_;
    poebot::gui::HotkeyCapture            capture_;
};

}  // namespace poebot::gui::panels
