#pragma once
#include <poebot/coords.hpp>

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

namespace poebot::win {

// Lightweight handle to a discovered game window. Stores the raw HWND; all
// queries forward to Win32 and are cheap (no allocated state).
class GameWindow {
public:
    GameWindow() = default;
    explicit GameWindow(HWND h) noexcept : hwnd_(h) {}

    // Search for a visible top-level window whose title contains `substr`
    // (UTF-8). Returns true if found (and stores the HWND internally).
    // NOTE: POE2's title ("Path of Exile 2") contains POE1's pattern
    //       ("Path of Exile"). The caller must probe the longer pattern first.
    bool tryFind(std::string_view titleSubstr);

    void  clear() noexcept { hwnd_ = nullptr; }
    bool  valid() const noexcept { return hwnd_ != nullptr && ::IsWindow(hwnd_); }
    HWND  hwnd()  const noexcept { return hwnd_; }

    // Window title as UTF-8, or "" if invalid.
    std::string title() const;

    // Client-area size (width, height). Returns {0,0} if invalid.
    struct Size { int w = 0; int h = 0; };
    Size clientSize() const;

    // Coordinate conversions between the game's client space and the screen.
    ScreenPoint clientToScreen(ClientPoint p) const;
    ClientPoint screenToClient(ScreenPoint p) const;

    // Bring the window to the foreground. Uses a SetForegroundWindow +
    // AttachThreadInput trick that works even if the tool window is focused.
    void activate() const;
    bool isForeground() const;

private:
    HWND hwnd_ = nullptr;
};

}  // namespace poebot::win
