#include <poebot/gui/panels/deposit_panel.hpp>

#include <poebot/i18n/i18n.hpp>
#include <poebot/task/task_runner.hpp>

#include <imgui.h>

#include <string>

namespace poebot::gui::panels {

void DepositPanel::render(PanelContext& ctx) {
    using poebot::i18n::tr;
    if (!ctx.settings || !ctx.settings->active()) {
        ImGui::TextUnformatted(tr("common.no_active_profile"));
        return;
    }
    auto& d = ctx.settings->active()->deposit;

    bool dirty = false;
    ImGui::SliderInt(tr("deposit.inv_columns"), &d.cols, 1, 24);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    ImGui::SliderInt(tr("deposit.inv_rows"), &d.rows, 1, 12);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;

    ImGui::Separator();
    ImGui::TextWrapped(tr("deposit.description_fmt"), d.cols, d.rows);

    ImGui::Spacing();

    // Task display. Start / Stop are hotkey-only (default F3 / End); the
    // footer below shows the live bindings.
    auto* runner = ctx.taskRunner;
    if (runner) {
        using poebot::task::RunnerState;
        const auto state = runner->state();

        if (state == RunnerState::Running || state == RunnerState::Stopping) {
            auto prog = runner->progress();
            ImGui::Text(tr("deposit.progress_fmt"), prog.currentItem, prog.totalItems);
            ImGui::ProgressBar(
                prog.totalItems > 0
                    ? static_cast<float>(prog.currentItem) / static_cast<float>(prog.totalItems)
                    : 0.0f);
            if (state == RunnerState::Stopping) {
                ImGui::TextDisabled("%s", tr("common.stopping"));
            }
        } else if (state == RunnerState::Finished) {
            // App loop auto-joins; until that happens the panel just
            // shows the "deposit finished" line.
            ImGui::TextUnformatted(tr("deposit.finished"));
        }
        // Idle state: nothing extra — slider rows + the hint footer below
        // are enough; no Start button.
    }

    // Hotkey hint footer — always visible (defaults: F3 to start, End to stop).
    ImGui::Spacing();
    const std::string startBind = poebot::gui::hotkeyLabel(ctx, "task.start.deposit");
    const std::string stopBind  = poebot::gui::hotkeyLabel(ctx, "task.stop");
    ImGui::TextDisabled(tr("task.hotkey_hint_fmt"),
                        startBind.c_str(), stopBind.c_str());

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
