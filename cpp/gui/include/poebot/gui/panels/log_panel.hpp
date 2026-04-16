#pragma once
#include <poebot/gui/panel.hpp>

#include <cstddef>

namespace poebot::gui::panels {

class LogPanel : public Panel {
public:
    const char* name() const override { return "Log"; }
    PanelKind   kind() const override { return PanelKind::Log; }
    void        render(PanelContext& ctx) override;

private:
    bool        autoScroll_ = true;
    std::size_t lastSize_   = 0;
};

}  // namespace poebot::gui::panels
