#include <poebot/gui/panels/map_panel.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/gui/affix_library_widget.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/task/task_runner.hpp>

#include <imgui.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace poebot::gui::panels {

void MapPanel::render(PanelContext& ctx) {
    using poebot::i18n::tr;
    if (!ctx.settings || !ctx.settings->active()) {
        ImGui::TextUnformatted(tr("common.no_active_profile"));
        return;
    }
    auto* prof = ctx.settings->active();
    auto& m    = prof->map;

    bool dirty = false;

    // Mode radio buttons
    int mode = static_cast<int>(m.mode);
    if (ImGui::RadioButton(tr("map.mode.alch_scour"), &mode, 1)) dirty = true;
    ImGui::SameLine();
    if (ImGui::RadioButton(tr("map.mode.chaos"), &mode, 2)) dirty = true;
    m.mode = (mode == 2) ? poebot::config::MapMode::Chaos
                         : poebot::config::MapMode::AlchAndScour;

    dirty |= ImGui::Checkbox(tr("map.batch_mode"), &m.batch);

    // Rows/cols only make sense when batching — single-map mode rolls one
    // item, so the grid dimensions don't apply.
    ImGui::BeginDisabled(!m.batch);
    ImGui::SliderInt(tr("map.columns"), &m.cols, 1, 12);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    ImGui::SliderInt(tr("map.rows"), &m.rows, 1, 12);
    if (ImGui::IsItemDeactivatedAfterEdit()) dirty = true;
    ImGui::EndDisabled();

    // See craft_panel for context: editing moved to the external editor;
    // the widget here only binds + reloads. Map uses its own subfolder so
    // its library list stays free of craft templates.
    {
        bool libChanged = false;
        const std::filesystem::path dir = ctx.affixLibraryDir
            ? (*ctx.affixLibraryDir / "map")
            : std::filesystem::path{};
        poebot::gui::affixLibraryWidget(dir, m.affixLibrary, m.affixes,
                                        "map", &libChanged);
        if (libChanged) dirty = true;
    }

    ImGui::Separator();

    // Task display. Start / Stop are hotkey-only (default F2 / End); the
    // footer below shows the live bindings.
    auto* runner = ctx.taskRunner;
    if (runner) {
        using poebot::task::RunnerState;
        const auto state = runner->state();
        const bool ours  = std::string_view(runner->taskName()) == "Map";

        if (state == RunnerState::Idle) {
            ImGui::Text(tr("map.stats_fmt"), prof->stats.mapOps, prof->stats.mapHits);
            ImGui::SameLine();
            if (ImGui::SmallButton(tr("map.reset_stats"))) {
                prof->stats.mapOps  = 0;
                prof->stats.mapHits = 0;
                dirty = true;
            }
        } else if (ours) {
            auto prog = runner->progress();
            ImGui::Text(tr("map.progress_fmt"),
                        prog.currentItem, prog.totalItems, prog.ops, prog.hits);
            ImGui::ProgressBar(
                prog.totalItems > 0
                    ? static_cast<float>(prog.currentItem) / static_cast<float>(prog.totalItems)
                    : 0.0f);
            if (state == RunnerState::Stopping) {
                ImGui::TextDisabled("%s", tr("common.stopping"));
            }
        } else {
            ImGui::TextDisabled(tr("common.other_task_running_fmt"), runner->taskName());
        }
    }

    // Hotkey hint footer — always visible so the user can see what they've
    // got bound (defaults: F2 to start, End to stop).
    ImGui::Spacing();
    const std::string startBind = poebot::gui::hotkeyLabel(ctx, "task.start.map");
    const std::string stopBind  = poebot::gui::hotkeyLabel(ctx, "task.stop");
    ImGui::TextDisabled(tr("task.hotkey_hint_fmt"),
                        startBind.c_str(), stopBind.c_str());

    if (dirty) ctx.dirty = true;
}

}  // namespace poebot::gui::panels
