// ============================================================
// POE Bot — entry point.
// Win32 subsystem; all logic lives in poebot::gui::App.
// ============================================================

#include <poebot/gui/app.hpp>

#include <windows.h>

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    poebot::gui::App app;
    return app.run(hInstance, nCmdShow);
}
