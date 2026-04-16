#pragma once

namespace poebot {

// Strong-typed coordinate systems. Distinguishing these at the type level
// prevents the most common bug in automation code: mixing up which coordinate
// space a value belongs to.
//
//   ClientPoint : relative to the game window client area (AHK CoordMode "Client")
//   ScreenPoint : absolute desktop screen coordinates
//   ImagePoint  : pixels inside a captured frame buffer (future vision module)

struct ClientPoint {
    int x = 0;
    int y = 0;
};

struct ScreenPoint {
    int x = 0;
    int y = 0;
};

struct ImagePoint {
    int x = 0;
    int y = 0;
};

// Offsets (deltas) between points of the same space. Keeps arithmetic
// type-safe: you can add an Offset to a Point, but not two Points.
struct ClientOffset {
    int dx = 0;
    int dy = 0;
};

constexpr bool operator==(ClientPoint a, ClientPoint b) noexcept { return a.x == b.x && a.y == b.y; }
constexpr bool operator!=(ClientPoint a, ClientPoint b) noexcept { return !(a == b); }
constexpr bool operator==(ScreenPoint a, ScreenPoint b) noexcept { return a.x == b.x && a.y == b.y; }
constexpr bool operator!=(ScreenPoint a, ScreenPoint b) noexcept { return !(a == b); }
constexpr bool operator==(ImagePoint a, ImagePoint b) noexcept { return a.x == b.x && a.y == b.y; }
constexpr bool operator!=(ImagePoint a, ImagePoint b) noexcept { return !(a == b); }

constexpr ClientPoint operator+(ClientPoint p, ClientOffset o) noexcept {
    return {p.x + o.dx, p.y + o.dy};
}
constexpr ClientOffset operator-(ClientPoint a, ClientPoint b) noexcept {
    return {a.x - b.x, a.y - b.y};
}

// Sentinel meaning "not set" for GUI display. A (0,0) coordinate is ambiguous
// (could be a legitimate top-left position), so we treat it as unset for now —
// matches the AHK script's convention. Revisit when we support negative coords
// legitimately (e.g., multi-monitor game windows).
constexpr bool isUnset(ClientPoint p) noexcept { return p.x == 0 && p.y == 0; }

}  // namespace poebot
