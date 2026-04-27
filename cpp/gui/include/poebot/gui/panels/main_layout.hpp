#pragma once
#include <poebot/gui/panel.hpp>

#include <memory>
#include <vector>

namespace poebot::gui::panels {

// Renders a single full-viewport host window containing:
//   - a top bar with the [PoE 1 | PoE 2] segmented profile switch and
//     right-aligned status (capture countdown + game-window state)
//   - a left sidebar with each Tab-kind panel (incl. the Appearance/Language
//     FAB at its bottom-right)
//   - the active tab panel in the middle
//   - a fixed-width log strip on the right with the Log-kind panel
// Closing is owned by the host window (title-bar X → WM_QUIT), so this
// layout no longer needs a wantExit pipe-through.
void renderMainLayout(const std::vector<std::unique_ptr<Panel>>& panels,
                      PanelContext& ctx);

}  // namespace poebot::gui::panels
