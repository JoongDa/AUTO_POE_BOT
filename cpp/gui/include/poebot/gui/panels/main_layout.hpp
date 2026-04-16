#pragma once
#include <poebot/gui/panel.hpp>

#include <memory>
#include <vector>

namespace poebot::gui::panels {

// Renders a single full-viewport host window containing:
//   - a menu bar (Profile / Language / App)
//   - a tab bar with each Tab-kind panel
//   - a fixed-height log strip at the bottom with the Log-kind panel
// Phase 3 can upgrade this to a proper DockSpace once panels have content
// worth rearranging.
void renderMainLayout(const std::vector<std::unique_ptr<Panel>>& panels,
                      PanelContext& ctx,
                      bool& wantExit);

}  // namespace poebot::gui::panels
