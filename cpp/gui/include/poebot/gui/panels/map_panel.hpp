#pragma once
#include <poebot/gui/panel.hpp>
#include <poebot/i18n/i18n.hpp>

namespace poebot::gui::panels {

class MapPanel : public Panel {
public:
    const char* name()  const override { return "Map"; }
    const char* label() const override { return poebot::i18n::tr("panel.map"); }
    void        render(PanelContext& ctx) override;
};

}  // namespace poebot::gui::panels
