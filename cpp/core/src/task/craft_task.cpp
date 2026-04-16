#include <poebot/task/craft_task.hpp>

#include <poebot/grid/grid.hpp>
#include <poebot/input/mouse.hpp>
#include <poebot/task/task_util.hpp>

#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

namespace poebot::task {

CraftTask::CraftTask(Params p)
    : params_(std::move(p)), matcher_(params_.craft.affixes) {}

void CraftTask::run(std::atomic<bool>& stop, ProgressCallback report) {
    const auto* gw = params_.gameWindow;
    if (!gw || !gw->valid()) { spdlog::warn("craft: game window not found"); return; }
    if (!matcher_.valid()) { spdlog::warn("craft: invalid or empty affix pattern"); return; }

    const auto& c  = params_.craft;
    const auto& co = params_.coords;

    const int totalItems = c.batch ? (c.cols * c.rows) : 1;
    const ScreenPoint orbSP = gw->clientToScreen(co.orb1);

    TaskProgress prog;
    prog.totalItems = totalItems;

    spdlog::info("craft: starting (batch={}, items={}, pattern='{}')",
                 c.batch, totalItems, c.affixes);

    for (int idx = 0; idx < totalItems && !stop.load(); ++idx) {
        const int row = c.batch ? (idx / c.cols) : 0;
        const int col = c.batch ? (idx % c.cols) : 0;

        const ClientPoint itemCP = grid::itemAt(co.baseItem, co.p01Item, co.p10Item, row, col);
        const ScreenPoint itemSP = gw->clientToScreen(itemCP);

        prog.currentItem = idx + 1;

        // Roll this item until affixes match.
        while (!stop.load()) {
            if (!gw->valid()) { spdlog::warn("craft: game window lost"); return; }

            // Pick up currency.
            input::mouse::moveAndClick(orbSP, input::mouse::Button::Right);
            if (!interruptibleSleep(stop, 50ms)) return;

            // Apply to item.
            input::mouse::moveAndClick(itemSP, input::mouse::Button::Left);
            if (!interruptibleSleep(stop, 200ms)) return;

            prog.ops++;
            report(prog);

            // Hover + read item text.
            input::mouse::moveTo(itemSP);
            if (!interruptibleSleep(stop, 100ms)) return;

            auto text = readItemClipboard(stop, 500ms);
            if (!text) continue;   // clipboard timeout — retry

            if (matcher_.matches(*text)) {
                prog.hits++;
                report(prog);
                spdlog::info("craft: HIT on ({},{}) after {} ops", row, col, prog.ops);
                break;
            }
        }
    }

    spdlog::info("craft: done — ops={}, hits={}", prog.ops, prog.hits);
}

}  // namespace poebot::task
