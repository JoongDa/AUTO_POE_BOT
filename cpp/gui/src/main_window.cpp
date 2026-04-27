#include <poebot/gui/main_window.hpp>
#include <poebot/gui/resource.h>

#include <dwmapi.h>
#include <spdlog/spdlog.h>

// DWMWA_USE_IMMERSIVE_DARK_MODE was renumbered between Win10 1809 (19)
// and Win10 2004+ (20). Trying both is the robust thing to do.
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif

namespace poebot::gui {

MainWindow::~MainWindow() {
    if (hwnd_) {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (hInstance_) {
        ::UnregisterClassW(className_, hInstance_);
        hInstance_ = nullptr;
    }
}

bool MainWindow::create(HINSTANCE hInstance, const wchar_t* title, int width, int height) {
    hInstance_ = hInstance;

    // Icon resource is embedded via gui/res/app.rc. LoadIconW returns null
    // silently when the .rc / .ico hasn't been built yet; we fall back to
    // the default in that case rather than aborting.
    HICON appIcon = ::LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = &MainWindow::WndProcStatic;
    wc.hInstance     = hInstance;
    wc.lpszClassName = className_;
    wc.hIcon         = appIcon;
    wc.hIconSm       = appIcon;
    if (!::RegisterClassExW(&wc)) {
        spdlog::error("RegisterClassExW failed: {}", ::GetLastError());
        return false;
    }

    // Overlay-style window flags, modeled on PoE Overlay (Electron's
    // alwaysOnTop:'screen-saver' + focusable:false collapses to the same
    // Win32 set):
    //   WS_EX_TOPMOST     — pin above the game's borderless-fullscreen window
    //   WS_EX_NOACTIVATE  — clicks don't steal focus, so the game never
    //                       sees a foreground change and never minimizes
    //   WS_EX_TOOLWINDOW  — keep the bot out of Alt+Tab and the taskbar; F9
    //                       (registered later) is the only way to hide/show
    // The matching WM_MOUSEACTIVATE handler (below) is required — without
    // it Win32 still tries to activate the window on click and overrides
    // WS_EX_NOACTIVATE for that one click.
    constexpr DWORD kOverlayExStyle =
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    hwnd_ = ::CreateWindowExW(
        kOverlayExStyle, className_, title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, hInstance, this);
    if (!hwnd_) {
        spdlog::error("CreateWindowExW failed: {}", ::GetLastError());
        return false;
    }

    return true;
}

void MainWindow::setDarkTitleBar(bool dark) {
    if (!hwnd_) return;
    BOOL v = dark ? TRUE : FALSE;
    if (FAILED(::DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                       &v, sizeof(v)))) {
        ::DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1,
                                &v, sizeof(v));
    }
}

void MainWindow::show(int nCmdShow) {
    ::ShowWindow(hwnd_, nCmdShow);
    ::UpdateWindow(hwnd_);
}

bool MainWindow::pumpMessages() {
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) return false;
    }
    return true;
}

LRESULT CALLBACK MainWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
        auto* self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    }
    auto* self = reinterpret_cast<MainWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->handleMessage(hwnd, msg, w, l);
    return ::DefWindowProcW(hwnd, msg, w, l);
}

LRESULT MainWindow::handleMessage(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    // ImGui's wndproc hook consumes mouse/keyboard messages it needs.
    if (filter_ && filter_(hwnd, msg, w, l)) {
        return 1;
    }
    switch (msg) {
        case WM_SIZE:
            if (w != SIZE_MINIMIZED && onResize_) {
                onResize_(LOWORD(l), HIWORD(l));
            }
            return 0;
        case WM_MOUSEACTIVATE:
            // Refuse activation on click. WS_EX_NOACTIVATE on the window
            // alone is not enough — when the user clicks the client area,
            // Win32 sends WM_MOUSEACTIVATE first and DefWindowProc returns
            // MA_ACTIVATE, briefly stealing focus from the game (which can
            // pop a fullscreen-game out of its borderless state on some
            // drivers). Returning MA_NOACTIVATE here closes that hole; the
            // click itself is still delivered as WM_LBUTTONDOWN to ImGui.
            return MA_NOACTIVATE;
        case WM_SYSCOMMAND:
            // Disable ALT-activated application menu (steals focus in-game).
            if ((w & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hwnd, msg, w, l);
}

}  // namespace poebot::gui
