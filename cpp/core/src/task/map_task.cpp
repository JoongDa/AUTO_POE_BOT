#include <poebot/task/map_task.hpp>

#include <poebot/grid/grid.hpp>
#include <poebot/input/mouse.hpp>
#include <poebot/task/task_util.hpp>

#include <spdlog/spdlog.h>

using namespace std::chrono_literals;
using poebot::config::MapMode;

namespace poebot::task {

MapTask::MapTask(Params p)
    : params_(std::move(p)), matcher_(params_.map.affixes) {}

bool MapTask::applyOrb(std::atomic<bool>& stop, ScreenPoint orbSP, ScreenPoint itemSP) {
    input::mouse::moveAndClick(orbSP, input::mouse::Button::Right);
    if (!interruptibleSleep(stop, 50ms)) return false;
    input::mouse::moveAndClick(itemSP, input::mouse::Button::Left);
    return interruptibleSleep(stop, 200ms);
}

std::optional<std::string> MapTask::readItem(std::atomic<bool>& stop, ScreenPoint itemSP) {
    input::mouse::moveTo(itemSP);
    if (!interruptibleSleep(stop, 100ms)) return std::nullopt;
    return readItemClipboard(stop, 500ms);
}

void MapTask::run(std::atomic<bool>& stop, ProgressCallback report) {
    const auto* gw = params_.gameWindow;
    if (!gw || !gw->valid()) { spdlog::warn("map: game window not found"); return; }
    if (!matcher_.valid()) { spdlog::warn("map: invalid or empty affix pattern"); return; }

    const auto& m  = params_.map;
    const auto& co = params_.coords;

    const int totalItems = m.batch ? (m.cols * m.rows) : 1;
    const ScreenPoint orb1SP = gw->clientToScreen(co.orb1);
    const ScreenPoint orb2SP = gw->clientToScreen(co.orb2);

    TaskProgress prog;
    prog.totalItems = totalItems;

    spdlog::info("map: starting (mode={}, batch={}, items={}, pattern='{}')",
                 static_cast<int>(m.mode), m.batch, totalItems, m.affixes);

    for (int idx = 0; idx < totalItems && !stop.load(); ++idx) {
        const int row = m.batch ? (idx / m.cols) : 0;
        const int col = m.batch ? (idx % m.cols) : 0;

        const ClientPoint itemCP = grid::itemAt(co.baseItem, co.p01Item, co.p10Item, row, col);
        const ScreenPoint itemSP = gw->clientToScreen(itemCP);

        prog.currentItem = idx + 1;

        while (!stop.load()) {
            if (!gw->valid()) { spdlog::warn("map: game window lost"); return; }

            if (m.mode == MapMode::AlchAndScour) {
                // Step 1: Alchemy
                if (!applyOrb(stop, orb1SP, itemSP)) return;
                prog.ops++;
                report(prog);

                auto text = readItem(stop, itemSP);
                if (!text) continue;

                if (matcher_.matches(*text)) {
                    prog.hits++;
                    report(prog);
                    spdlog::info("map: HIT on ({},{}) after {} ops", row, col, prog.ops);
                    break;
                }

                // Step 2: Scour
                if (!applyOrb(stop, orb2SP, itemSP)) return;
                if (!interruptibleSleep(stop, 100ms)) return;
                // Loop back to Alchemy.

            } else {
                // Chaos mode: just re-roll
                if (!applyOrb(stop, orb1SP, itemSP)) return;
                prog.ops++;
                report(prog);

                auto text = readItem(stop, itemSP);
                if (!text) continue;

                if (matcher_.matches(*text)) {
                    prog.hits++;
                    report(prog);
                    spdlog::info("map: HIT on ({},{}) after {} ops", row, col, prog.ops);
                    break;
                }
            }
        }
    }

    spdlog::info("map: done — ops={}, hits={}", prog.ops, prog.hits);
}

}  // namespace poebot::task
