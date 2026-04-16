#pragma once
#include <poebot/gui/panel.hpp>

namespace poebot::gui::panels {

class MapPanel : public Panel {
public:
    const char* name() const override { return "Map"; }
    void        render(PanelContext& ctx) override;
};

}  // namespace poebot::gui::panels
