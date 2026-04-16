#pragma once
#include <poebot/gui/panel.hpp>

namespace poebot::gui::panels {

class CraftPanel : public Panel {
public:
    const char* name() const override { return "Craft"; }
    void        render(PanelContext& ctx) override;
};

}  // namespace poebot::gui::panels
