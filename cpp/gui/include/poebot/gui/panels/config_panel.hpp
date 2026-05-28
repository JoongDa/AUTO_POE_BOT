#pragma once
#include <poebot/coords.hpp>
#include <poebot/gui/hotkey_capture.hpp>
#include <poebot/gui/panel.hpp>
#include <poebot/i18n/i18n.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace poebot::gui::panels {

// UI state for the template-crop modal. Lives as a value member in
// ConfigPanel so it persists across frames without heap allocation.
// Pixels are in BGRA format, normalized to 1920×1080 at capture time.
struct CropState {
    bool        active       = false;
    std::string templateName;

    // D3D11 SRV for the screenshot texture — stored as void* to keep
    // this header free of d3d11.h; cast to ID3D11ShaderResourceView*
    // inside config_panel.cpp.
    void*       texSRV = nullptr;

    // Normalized screenshot (1920×1080, BGRA)
    std::vector<uint8_t> pixels;
    int         scrW = 0, scrH = 0, scrStride = 0;

    // Display metrics (computed once when the modal first renders)
    float       dispScale = 1.0f;   // pixels-in-modal / pixels-in-1080p
    float       dispW     = 0.0f;
    float       dispH     = 0.0f;

    // Selection corners in *display* coords (not 1080p)
    float       selAx = 0.0f, selAy = 0.0f;
    float       selBx = 0.0f, selBy = 0.0f;
    bool        hasSel = false;
};

// Settings panel — Hotkeys tab (global hotkeys only) + Coordinates tab
// (per-profile coords with rebind buttons that target each coord's
// capture.* hotkey). `name()` stays "Config" (stable internal id used
// as activePanel key); the user-facing label flips to "Settings" /
// "设置" via the i18n table.
class ConfigPanel : public Panel {
public:
    ~ConfigPanel();   // releases thumbnail SRVs
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
    void coordRow(PanelContext& ctx, const char* name, poebot::ClientPoint& p, const int* qty = nullptr);

    // Common click handler for any binding button on this panel — sets
    // rebind state and arms the keyboard hook. Called from coordRow and
    // from the Hotkeys-tab inline loop.
    void requestRebind(const char* actionId);

    // Render the Auto Calibrate tab body. Lazy-loads the template library on
    // first call and delegates calibration to runAutoCalibrate().
    void renderAutoCalibrate(PanelContext& ctx);

    // Synchronously capture the game window, normalize to 1920×1080, run
    // cv::matchTemplate for every loaded template, and write matches above
    // kMinScore into the active profile. Updates calibStatus_ when done.
    void runAutoCalibrate(PanelContext& ctx);

    // Template thumbnail SRV cache — one entry per lib->entries() slot.
    // Rebuilt whenever ctx.templates changes (i.e. on Reload or profile switch).
    // Stored as void* to keep d3d11.h out of this header.
    void rebuildTemplThumbs(PanelContext& ctx);
    void releaseTemplThumbs();

    // Template crop modal — shown when the user clicks a template name button.
    // Captures the current screen (normalized to 1920×1080), lets the user
    // drag-select a region, and saves it as the new PNG for that template.
    void renderCropModal(PanelContext& ctx);
    void closeCropModal();
    void saveCroppedTemplate(PanelContext& ctx);

    // Rebind modal state. While `rebindingId_` is non-empty the modal is
    // open and the hook in `capture_` is intercepting all keyboard input
    // process-wide. ESC or a successful commit clears this back to "".
    std::string                           rebindingId_;
    std::string                           rebindError_;
    poebot::gui::HotkeyCapture            capture_;

    // Auto-calibrate UI state. The template library itself lives in App
    // (shared via PanelContext::templates); this panel just renders it
    // and writes results into the active profile's `calibrated` map.
    std::string                           calibStatus_;
    std::vector<void*>                    templThumbSRVs_;
    const poebot::vision::TemplateLibrary* cachedTemplLib_ = nullptr;

    // Template crop modal state.
    CropState                             cropState_;
};

}  // namespace poebot::gui::panels
