#include <poebot/gui/panels/deposit_panel.hpp>

#include <poebot/i18n/i18n.hpp>
#include <poebot/task/deposit_task.hpp>
#include <poebot/task/task_runner.hpp>
#include <poebot/win/window.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

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

    // Task controls
    auto* runner = ctx.taskRunner;
    if (runner) {
        using poebot::task::RunnerState;
        const auto state = runner->state();

        if (state == RunnerState::Idle) {
            if (ImGui::Button(tr("deposit.start"))) {
                auto* prof = ctx.settings->active();
                if (!prof) {
                    spdlog::warn("deposit: no active profile");
                } else if (!ctx.gameWindow || !ctx.gameWindow->valid()) {
                    spdlog::warn("deposit: game window not found — launch POE first");
                } else {
                    poebot::task::DepositTask::Params p;
                    p.gameWindow = ctx.gameWindow;
                    p.deposit    = prof->deposit;
                    p.coords     = prof->coords;
                    runner->start(std::make_unique<poebot::task::DepositTask>(std::move(p)));
                }
            }
        } else if (state == RunnerState::Running || state == RunnerState::Stopping) {
            auto prog = runner->progress();
            ImGui::Text(tr("deposit.progress_fmt"), prog.currentItem, prog.totalItems);
            ImGui::ProgressBar(
                prog.totalItems > 0
                    ? static_cast<float>(prog.currentItem) / static_cast<float>(prog.totalItems)
                    : 0.0f);

            if (state == RunnerState::Running) {
                if (ImGui::Button(tr("common.stop"))) runner->requestStop();
            } else {
                ImGui::BeginDisabled(true);
                ImGui::Button(tr("common.stopping"));
                ImGui::EndDisabled();
            }
        } else {
            // Finished — App loop will auto-join.
            ImGui::TextUnformatted(tr("deposit.finished"));
        }
    }

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
