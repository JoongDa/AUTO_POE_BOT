#pragma once
#include <poebot/gui/panel.hpp>
#include <poebot/i18n/i18n.hpp>

namespace poebot::gui::panels {

class DepositPanel : public Panel {
public:
    const char* name()  const override { return "Deposit"; }
    const char* label() const override { return poebot::i18n::tr("panel.deposit"); }
    void        render(PanelContext& ctx) override;
};

}  // namespace poebot::gui::panels
