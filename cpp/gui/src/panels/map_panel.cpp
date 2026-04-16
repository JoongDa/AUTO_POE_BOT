#include <poebot/gui/panels/map_panel.hpp>

#include <poebot/config/profile.hpp>

#include <imgui.h>

#include <cfloat>
#include <cstdio>

namespace poebot::gui::panels {

namespace {
constexpr size_t kAffixBufCap = 4096;
}

void MapPanel::render(PanelContext& ctx) {
    if (!ctx.settings || !ctx.settings->active()) {
        ImGui::TextUnformatted("No active profile.");
        return;
    }
    auto* prof = ctx.settings->active();
    auto& m    = prof->map;

    bool dirty = false;

    // Mode radio buttons
    int mode = static_cast<int>(m.mode);
    if (ImGui::RadioButton("炼金+洗白 (Alch+Scour)", &mode, 1)) dirty = true;
    ImGui::SameLine();
    if (ImGui::RadioButton("混沌石 (Chaos)", &mode, 2)) dirty = true;
    m.mode = (mode == 2) ? poebot::config::MapMode::Chaos
                         : poebot::config::MapMode::AlchAndScour;

    dirty |= ImGui::Checkbox("Batch mode", &m.batch);
    ImGui::SliderInt("Columns", &m.cols, 1, 12);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    ImGui::SliderInt("Rows", &m.rows, 1, 12);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;

    char buf[kAffixBufCap];
    std::snprintf(buf, sizeof(buf), "%s", m.affixes.c_str());
    if (ImGui::InputTextMultiline("Affixes (|-separated regex)",
                                  buf, sizeof(buf),
                                  ImVec2(-FLT_MIN, 120))) {
        m.affixes = buf;
        dirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Stats  ops=%d  hits=%d", prof->stats.mapOps, prof->stats.mapHits);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset stats")) {
        prof->stats.mapOps  = 0;
        prof->stats.mapHits = 0;
        dirty = true;
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(true);
    ImGui::Button("Start (Phase 3)");
    ImGui::SameLine();
    ImGui::Button("Stop (Phase 3)");
    ImGui::EndDisabled();

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
