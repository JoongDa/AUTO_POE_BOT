// ============================================================
// POE Bot — entry point.
// Win32 subsystem; all logic lives in poebot::gui::App.
// ============================================================

#include <poebot/gui/app.hpp>

#include <windows.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // Declare per-monitor v2 DPI awareness before any window is created.
    // Without this, Windows applies bitmap-stretch scaling to the entire
    // app on HiDPI monitors — the result is blurry and all our carefully
    // sized padding/font values are off by the DPI scale factor.
    // PER_MONITOR_AWARE_V2 tells Win32 "I manage my own scaling"; our
    // render pipeline (D3D11 + ImGui) then draws at true physical resolution,
    // matching the Mac Retina 2× principle: same logical canvas, more pixels.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    poebot::gui::App app;
    return app.run(hInstance, nCmdShow);
}
