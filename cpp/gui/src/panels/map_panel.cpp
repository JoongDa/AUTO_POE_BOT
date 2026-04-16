#include <poebot/gui/panels/map_panel.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/task/map_task.hpp>
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

    // Task controls
    auto* runner = ctx.taskRunner;
    if (runner) {
        using poebot::task::RunnerState;
        const auto state = runner->state();
        const bool ours  = std::string_view(runner->taskName()) == "Map";

        if (state == RunnerState::Idle) {
            ImGui::Text("Stats  ops=%d  hits=%d", prof->stats.mapOps, prof->stats.mapHits);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset stats")) {
                prof->stats.mapOps  = 0;
                prof->stats.mapHits = 0;
                dirty = true;
            }
            ImGui::Spacing();

            if (ImGui::Button("Start map roll")) {
                if (!ctx.gameWindow || !ctx.gameWindow->valid()) {
                    spdlog::warn("map: game window not found — launch POE first");
                } else if (m.affixes.empty()) {
                    spdlog::warn("map: affix pattern is empty");
                } else {
                    poebot::task::MapTask::Params p;
                    p.gameWindow = ctx.gameWindow;
                    p.map        = m;
                    p.coords     = prof->coords;
                    runner->start(std::make_unique<poebot::task::MapTask>(std::move(p)));
                }
            }
        } else if (ours) {
            auto prog = runner->progress();
            ImGui::Text("Rolling: map %d / %d  |  ops=%d  hits=%d",
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
