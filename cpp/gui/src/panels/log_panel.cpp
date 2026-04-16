#include <poebot/gui/panels/log_panel.hpp>

#include <imgui.h>

namespace poebot::gui::panels {

namespace {

// spdlog level values: trace=0, debug=1, info=2, warn=3, err=4, critical=5.
ImVec4 colorForLevel(int lvl) {
    switch (lvl) {
        case 0:  return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);  // trace — grey
        case 1:  return ImVec4(0.60f, 0.80f, 1.00f, 1.0f);  // debug — cyan
        case 2:  return ImVec4(0.90f, 0.90f, 0.90f, 1.0f);  // info  — white
        case 3:  return ImVec4(1.00f, 0.85f, 0.30f, 1.0f);  // warn  — yellow
        case 4:  return ImVec4(1.00f, 0.40f, 0.40f, 1.0f);  // error — red
        case 5:  return ImVec4(1.00f, 0.20f, 0.60f, 1.0f);  // crit  — magenta
        default: return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

}  // namespace

void LogPanel::render(PanelContext& ctx) {
    ImGui::TextUnformatted("Log");
    ImGui::SameLine();
    ImGui::Checkbox("Auto scroll", &autoScroll_);
    ImGui::SameLine();
    if (ImGui::Button("Clear") && ctx.logSink) {
        ctx.logSink->clear();
        lastSize_ = 0;
    }
    ImGui::SameLine();
    if (ctx.logSink) {
        ImGui::Text("(%zu entries)", ctx.logSink->size());
    }
    ImGui::Separator();

    ImGui::BeginChild("##LogScroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (ctx.logSink) {
        auto entries = ctx.logSink->snapshot();
        for (const auto& e : entries) {
            ImGui::PushStyleColor(ImGuiCol_Text, colorForLevel(e.level));
            ImGui::TextUnformatted(e.text.c_str());
            ImGui::PopStyleColor();
        }
        if (autoScroll_ && entries.size() != lastSize_) {
            ImGui::SetScrollHereY(1.0f);
            lastSize_ = entries.size();
        }
    }
    ImGui::EndChild();
}

}  // namespace poebot::gui::panels
