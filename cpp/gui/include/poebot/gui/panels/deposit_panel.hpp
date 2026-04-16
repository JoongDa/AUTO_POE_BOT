#pragma once
#include <poebot/gui/panel.hpp>

namespace poebot::gui::panels {

class DepositPanel : public Panel {
public:
    const char* name() const override { return "Deposit"; }
    void        render(PanelContext& ctx) override;
};

}  // namespace poebot::gui::panels
