#pragma once
#include <poebot/gui/panel.hpp>
#include <poebot/i18n/i18n.hpp>

#include <cstddef>

namespace poebot::gui::panels {

class LogPanel : public Panel {
public:
    const char* name()  const override { return "Log"; }
    const char* label() const override { return poebot::i18n::tr("panel.log"); }
    PanelKind   kind()  const override { return PanelKind::Log; }
    void        render(PanelContext& ctx) override;

private:
    bool        autoScroll_ = true;
    std::size_t lastSize_   = 0;
};

}  // namespace poebot::gui::panels
