#pragma once
#include <poebot/coords.hpp>

namespace poebot::grid {

// Compute the client-space centre of item at (row, col) in a grid whose
// geometry is defined by three anchor points:
//   base  — the item at (0, 0)
//   p01   — the item at (0, 1); difference from base gives the X step
//   p10   — the item at (1, 0); difference from base gives the Y step
// Used for both the stash/craft grid and the inventory grid.

inline ClientPoint itemAt(ClientPoint base, ClientPoint p01, ClientPoint p10,
                          int row, int col) noexcept {
    const int dx = p01.x - base.x;   // pixels per column
    const int dy = p10.y - base.y;   // pixels per row
    return {base.x + col * dx, base.y + row * dy};
}

// Convenience alias using inventory-specific anchor names.
inline ClientPoint invAt(ClientPoint invBase, ClientPoint invP01, ClientPoint invP10,
                         int row, int col) noexcept {
    return itemAt(invBase, invP01, invP10, row, col);
}

}  // namespace poebot::grid
