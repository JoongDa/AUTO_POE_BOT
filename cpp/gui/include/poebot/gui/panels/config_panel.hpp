#pragma once
#include <poebot/gui/panel.hpp>
#include <poebot/i18n/i18n.hpp>

namespace poebot::gui::panels {

class ConfigPanel : public Panel {
public:
    const char* name()  const override { return "Config"; }
    const char* label() const override { return poebot::i18n::tr("panel.config"); }
    void        render(PanelContext& ctx) override;
};

}  // namespace poebot::gui::panels
