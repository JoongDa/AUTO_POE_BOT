#pragma once
#include <windows.h>

#include <functional>

namespace poebot::gui {

// Owns the Win32 window class + HWND and drives message dispatch. Hostless:
// app code subscribes to resize / message events via setters so the Win32
// details don't leak upward.
class MainWindow {
public:
    // Return true if the pre-filter consumed the message and DefWindowProc
    // should be skipped. Use this to hook ImGui_ImplWin32_WndProcHandler.
    using MessageFilter = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;
    using ResizeHandler = std::function<void(UINT, UINT)>;

    MainWindow() = default;
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool create(HINSTANCE hInstance, const wchar_t* title, int width, int height);
    void show(int nCmdShow);

    // Drain pending messages. Returns false once WM_QUIT has been seen.
    bool pumpMessages();

    HWND hwnd() const noexcept { return hwnd_; }

    void setMessageFilter(MessageFilter f) { filter_   = std::move(f); }
    void setResizeHandler(ResizeHandler h) { onResize_ = std::move(h); }

private:
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(HWND, UINT, WPARAM, LPARAM);

    HINSTANCE      hInstance_ = nullptr;
    HWND           hwnd_      = nullptr;
    const wchar_t* className_ = L"POEBotWndClass";

    MessageFilter filter_;
    ResizeHandler onResize_;
};

}  // namespace poebot::gui
