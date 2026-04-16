#include <poebot/gui/panels/deposit_panel.hpp>

#include <imgui.h>

namespace poebot::gui::panels {

void DepositPanel::render(PanelContext& ctx) {
    if (!ctx.settings || !ctx.settings->active()) {
        ImGui::TextUnformatted("No active profile.");
        return;
    }
    auto& d = ctx.settings->active()->deposit;

    bool dirty = false;
    ImGui::SliderInt("Inventory columns", &d.cols, 1, 24);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    ImGui::SliderInt("Inventory rows", &d.rows, 1, 12);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;

    ImGui::Separator();
    ImGui::TextWrapped(
        "Scans the inventory grid (%d x %d) and shift-clicks each occupied cell "
        "into the open stash tab. Wiring comes in Phase 3.",
        d.cols, d.rows);

    ImGui::Spacing();
    ImGui::BeginDisabled(true);
    ImGui::Button("Deposit now (Phase 3)");
    ImGui::EndDisabled();

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
