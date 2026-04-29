#pragma once
#include <windows.h>

#include <cstdint>
#include <optional>
#include <vector>

// Screen-pixel capture. Used by the OpenCV-backed currency-locator pipeline
// (BitBlt the game window or the entire virtual desktop into a memory DC,
// hand the resulting RGBA buffer to the matcher) and by the crop-overlay
// modal which needs a frozen snapshot of the screen to draw and crop on.
//
// Why BitBlt and not DXGI Desktop Duplication: BitBlt is a pure pixel read
// — no foreground / focus state changes — so we don't risk PoE's borderless
// window seeing a transition that some drivers turn into a fullscreen
// minimise. DXGI is faster but adds device setup, and we don't capture
// often enough for the perf to matter (template matching is a one-shot
// per task start, not per frame).
//
// All public functions return std::nullopt on any Win32 failure and log a
// warning via spdlog so the caller's "didn't get an image" branch is the
// single failure path; callers don't need to inspect GetLastError.
namespace poebot::sys {

struct CapturedImage {
    int                  width  = 0;
    int                  height = 0;
    int                  stride = 0;        // bytes per row, == width * 4
    std::vector<uint8_t> pixels;            // BGRA, top-down
};

// Capture the client area of `hwnd`. Useful for "snapshot the PoE inventory
// at task start" — the returned image is in PoE's client-space coordinates,
// so points on the image map directly to ClientPoint values without any
// further transform.
std::optional<CapturedImage> captureClient(HWND hwnd);

// Capture the entire virtual screen (all monitors stitched into one image).
// Used by the crop-overlay modal which needs to dim the whole desktop and
// let the user drag-select anywhere on it. The result's (0, 0) corresponds
// to the top-left corner of the virtual screen — which can be negative in
// Win32 absolute coords on multi-monitor setups, so the caller usually
// needs to track the virtual-screen origin separately.
struct VirtualScreenRect {
    int x = 0, y = 0;       // top-left in absolute screen coords (can be < 0)
    int width = 0, height = 0;
};
VirtualScreenRect virtualScreenRect();

std::optional<CapturedImage> captureVirtualScreen();

}  // namespace poebot::sys
