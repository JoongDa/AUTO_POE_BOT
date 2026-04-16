#pragma once
#include <poebot/gui/panel.hpp>

namespace poebot::gui::panels {

class ConfigPanel : public Panel {
public:
    const char* name() const override { return "Config"; }
    void        render(PanelContext& ctx) override;
};

}  // namespace poebot::gui::panels
