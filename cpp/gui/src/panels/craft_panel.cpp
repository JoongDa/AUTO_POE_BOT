#include <poebot/gui/panels/craft_panel.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cfloat>
#include <cstdio>

namespace poebot::gui::panels {

namespace {
// Fixed-size ImGui-friendly scratch buffer for affix regex editing. 4 KB is
// plenty for any realistic regex list; spilling above this would be a UX
// smell we'd want to catch anyway.
constexpr size_t kAffixBufCap = 4096;
}

void CraftPanel::render(PanelContext& ctx) {
    if (!ctx.settings || !ctx.settings->active()) {
        ImGui::TextUnformatted("No active profile.");
        return;
    }
    auto* prof = ctx.settings->active();
    auto& c    = prof->craft;

    bool dirty = false;

    dirty |= ImGui::Checkbox("Batch mode", &c.batch);

    if (ImGui::SliderInt("Columns", &c.cols, 1, 24)) {
        if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    }
    if (ImGui::SliderInt("Rows", &c.rows, 1, 12)) {
        if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    }
    // Any change on the slider also counts (catches keyboard edits etc.),
    // but saves are throttled so repeated drags are cheap.
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;

    // Affix regex editor. Copy settings string into a local buffer each
    // frame; on edit, commit back and flag dirty.
    char buf[kAffixBufCap];
    std::snprintf(buf, sizeof(buf), "%s", c.affixes.c_str());
    if (ImGui::InputTextMultiline("Affixes (|-separated regex)",
                                  buf, sizeof(buf),
                                  ImVec2(-FLT_MIN, 120))) {
        c.affixes = buf;
        dirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Stats  ops=%d  hits=%d", prof->stats.craftOps, prof->stats.craftHits);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset stats")) {
        prof->stats.craftOps  = 0;
        prof->stats.craftHits = 0;
        dirty = true;
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(true);  // Phase 3 wires to the task engine.
    ImGui::Button("Start (Phase 3)");
    ImGui::SameLine();
    ImGui::Button("Stop (Phase 3)");
    ImGui::EndDisabled();

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
