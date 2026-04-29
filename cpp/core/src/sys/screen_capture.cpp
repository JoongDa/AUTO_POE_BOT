#include <poebot/sys/screen_capture.hpp>

#include <spdlog/spdlog.h>

#include <cstring>

namespace poebot::sys {

namespace {

// Shared backend for BitBlt-into-DIB. Source DC + (srcX, srcY, w, h) are the
// only inputs that vary between client and virtual-screen capture; the rest
// (DIB section, BitBlt, byte copy) is identical. Splitting it lets the two
// public entry points stay short and read top-down.
std::optional<CapturedImage> captureFromDC(HDC sourceDC,
                                           int srcX, int srcY,
                                           int width, int height,
                                           const char* what) {
    if (width <= 0 || height <= 0) {
        spdlog::warn("screen_capture: {} has zero size ({}x{})", what, width, height);
        return std::nullopt;
    }

    HDC memDC = ::CreateCompatibleDC(sourceDC);
    if (!memDC) {
        spdlog::warn("screen_capture: {} CreateCompatibleDC failed: {}", what, ::GetLastError());
        return std::nullopt;
    }

    // Top-down DIB: negative biHeight tells GDI to lay rows out from top to
    // bottom in memory, matching most image libraries' expectations and
    // avoiding the manual Y-flip the default bottom-up layout requires.
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;            // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dibBits = nullptr;
    HBITMAP dib = ::CreateDIBSection(sourceDC, &bmi, DIB_RGB_COLORS,
                                     &dibBits, nullptr, 0);
    if (!dib || !dibBits) {
        spdlog::warn("screen_capture: {} CreateDIBSection failed: {}", what, ::GetLastError());
        if (dib) ::DeleteObject(dib);
        ::DeleteDC(memDC);
        return std::nullopt;
    }

    HGDIOBJ oldBmp = ::SelectObject(memDC, dib);

    // CAPTUREBLT lets us pull layered windows (e.g. PoE's tooltips) into the
    // capture instead of the empty rect that bare SRCCOPY would yield over
    // them. Marginal cost for the one-shot use case here.
    if (!::BitBlt(memDC, 0, 0, width, height,
                  sourceDC, srcX, srcY,
                  SRCCOPY | CAPTUREBLT)) {
        spdlog::warn("screen_capture: {} BitBlt failed: {}", what, ::GetLastError());
        ::SelectObject(memDC, oldBmp);
        ::DeleteObject(dib);
        ::DeleteDC(memDC);
        return std::nullopt;
    }

    // Force any pending GDI ops to land before we read the DIB bytes —
    // omitting this can hand the caller a partial frame on first capture.
    ::GdiFlush();

    CapturedImage out;
    out.width  = width;
    out.height = height;
    out.stride = width * 4;
    out.pixels.assign(static_cast<const uint8_t*>(dibBits),
                      static_cast<const uint8_t*>(dibBits) + (width * 4 * height));

    ::SelectObject(memDC, oldBmp);
    ::DeleteObject(dib);
    ::DeleteDC(memDC);
    return out;
}

}  // namespace

std::optional<CapturedImage> captureClient(HWND hwnd) {
    if (!hwnd || !::IsWindow(hwnd)) {
        spdlog::warn("screen_capture: captureClient called with invalid HWND");
        return std::nullopt;
    }

    RECT clientRect{};
    if (!::GetClientRect(hwnd, &clientRect)) {
        spdlog::warn("screen_capture: GetClientRect failed: {}", ::GetLastError());
        return std::nullopt;
    }
    const int width  = clientRect.right  - clientRect.left;
    const int height = clientRect.bottom - clientRect.top;

    HDC windowDC = ::GetDC(hwnd);
    if (!windowDC) {
        spdlog::warn("screen_capture: GetDC failed: {}", ::GetLastError());
        return std::nullopt;
    }

    auto img = captureFromDC(windowDC, 0, 0, width, height, "captureClient");
    ::ReleaseDC(hwnd, windowDC);
    return img;
}

VirtualScreenRect virtualScreenRect() {
    return VirtualScreenRect{
        ::GetSystemMetrics(SM_XVIRTUALSCREEN),
        ::GetSystemMetrics(SM_YVIRTUALSCREEN),
        ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
        ::GetSystemMetrics(SM_CYVIRTUALSCREEN),
    };
}

std::optional<CapturedImage> captureVirtualScreen() {
    const auto rect = virtualScreenRect();

    // Pulling from the screen DC (HWND == nullptr) is the standard "capture
    // the desktop" path — works on borderless-windowed PoE since DWM is
    // compositing the frame and we just read the composed result.
    HDC screenDC = ::GetDC(nullptr);
    if (!screenDC) {
        spdlog::warn("screen_capture: GetDC(NULL) failed: {}", ::GetLastError());
        return std::nullopt;
    }

    auto img = captureFromDC(screenDC, rect.x, rect.y, rect.width, rect.height,
                             "captureVirtualScreen");
    ::ReleaseDC(nullptr, screenDC);
    return img;
}

}  // namespace poebot::sys
