#include <poebot/gui/panels/craft_panel.hpp>

#include <poebot/task/craft_task.hpp>
#include <poebot/task/task_runner.hpp>
#include <poebot/win/window.hpp>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cfloat>
#include <cstdio>

namespace poebot::gui::panels {

namespace {
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
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;

    char buf[kAffixBufCap];
    std::snprintf(buf, sizeof(buf), "%s", c.affixes.c_str());
    if (ImGui::InputTextMultiline("Affixes (|-separated regex)",
                                  buf, sizeof(buf),
                                  ImVec2(-FLT_MIN, 120))) {
        c.affixes = buf;
        dirty = true;
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
            ImGui::Text("Stats  ops=%d  hits=%d", prof->stats.craftOps, prof->stats.craftHits);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset stats")) {
                prof->stats.craftOps  = 0;
                prof->stats.craftHits = 0;
                dirty = true;
            }
            ImGui::Spacing();

            if (ImGui::Button("Start craft")) {
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
            ImGui::Text("Crafting: item %d / %d  |  ops=%d  hits=%d",
                        prog.currentItem, prog.totalItems, prog.ops, prog.hits);
            ImGui::ProgressBar(
                prog.totalItems > 0
                    ? static_cast<float>(prog.currentItem) / static_cast<float>(prog.totalItems)
                    : 0.0f);

            if (state == RunnerState::Running) {
                if (ImGui::Button("Stop")) runner->requestStop();
            } else {
                ImGui::BeginDisabled(true);
                ImGui::Button("Stopping...");
                ImGui::EndDisabled();
            }
        } else {
            ImGui::TextDisabled("Another task is running: %s", runner->taskName());
        }
    }

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
