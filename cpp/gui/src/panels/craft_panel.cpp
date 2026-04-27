#include <poebot/gui/panels/craft_panel.hpp>

#include <poebot/gui/affix_library_widget.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/task/craft_task.hpp>
#include <poebot/task/task_runner.hpp>
#include <poebot/win/window.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cfloat>
#include <cstdio>
#include <filesystem>

namespace poebot::gui::panels {

void CraftPanel::render(PanelContext& ctx) {
    using poebot::i18n::tr;
    if (!ctx.settings || !ctx.settings->active()) {
        ImGui::TextUnformatted(tr("common.no_active_profile"));
        return;
    }
    auto* prof = ctx.settings->active();
    auto& c    = prof->craft;

    bool dirty = false;

    dirty |= ImGui::Checkbox(tr("craft.batch_mode"), &c.batch);

    // Rows/cols only make sense when batching — otherwise the task operates on
    // a single item and the grid dimensions are moot. Disable the sliders so
    // the UI makes that constraint obvious.
    ImGui::BeginDisabled(!c.batch);
    ImGui::SliderInt(tr("craft.columns"), &c.cols, 1, 24);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    ImGui::SliderInt(tr("craft.rows"), &c.rows, 1, 12);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    ImGui::EndDisabled();

    // Affixes: the inline textbox is gone — editing now happens in the
    // user's external editor (notepad / VSCode / …) on the .txt file
    // itself. The widget here only binds + previews the library; rules
    // are parsed from `c.affixes` at task-start time, same as before.
    // Craft and Map keep separate library pools (item mods vs map mods)
    // under sibling subfolders of the active profile's affix root.
    {
        bool libChanged = false;
        const std::filesystem::path dir = ctx.affixLibraryDir
            ? (*ctx.affixLibraryDir / "craft")
            : std::filesystem::path{};
        poebot::gui::affixLibraryWidget(dir, c.affixLibrary, c.affixes,
                                        "craft", &libChanged);
        if (libChanged) dirty = true;
    }

    ImGui::Separator();

    // Task controls
    auto* runner = ctx.taskRunner;
    if (runner) {
        using poebot::task::RunnerState;
        const auto state = runner->state();
        const bool ours  = std::string_view(runner->taskName()) == "Craft";

        if (state == RunnerState::Idle) {
            // Stats from last run / persisted stats
            ImGui::Text(tr("craft.stats_fmt"), prof->stats.craftOps, prof->stats.craftHits);
            ImGui::SameLine();
            if (ImGui::SmallButton(tr("craft.reset_stats"))) {
                prof->stats.craftOps  = 0;
                prof->stats.craftHits = 0;
                dirty = true;
            }
            ImGui::Spacing();

            if (ImGui::Button(tr("craft.start"))) {
                if (!ctx.gameWindow || !ctx.gameWindow->valid()) {
                    spdlog::warn("craft: game window not found — launch POE first");
                } else if (c.affixes.empty()) {
                    spdlog::warn("craft: affix pattern is empty");
                } else {
                    poebot::task::CraftTask::Params p;
                    p.gameWindow = ctx.gameWindow;
                    p.craft      = c;
                    p.coords     = prof->coords;
                    runner->start(std::make_unique<poebot::task::CraftTask>(std::move(p)));
                }
            }
        } else if (ours) {
            auto prog = runner->progress();
            ImGui::Text(tr("craft.progress_fmt"),
                        prog.currentItem, prog.totalItems, prog.ops, prog.hits);
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
            ImGui::TextDisabled(tr("common.other_task_running_fmt"), runner->taskName());
        }
    }

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
