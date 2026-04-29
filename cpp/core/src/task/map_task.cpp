#include <poebot/task/map_task.hpp>

#include <poebot/coords.hpp>
#include <poebot/grid/grid.hpp>
#include <poebot/input/motion.hpp>
#include <poebot/input/mouse.hpp>
#include <poebot/log/log_sink.hpp>
#include <poebot/task/task_util.hpp>

#include <spdlog/spdlog.h>

using namespace std::chrono_literals;
using poebot::config::MapMode;

namespace poebot::task {

MapTask::MapTask(Params p)
    : params_(std::move(p)), matcher_(params_.map.affixes) {}

bool MapTask::applyOrb(std::atomic<bool>& stop, ScreenPoint orbSP, ScreenPoint itemSP) {
    if (!input::mouse::humanClick(stop, orbSP, input::mouse::Button::Right)) return false;
    if (!input::mouse::humanClick(stop, itemSP, input::mouse::Button::Left)) return false;
    return input::motion::gaussSleep(stop, 200ms, 40ms, 120ms);
}

std::optional<std::string> MapTask::readItem(std::atomic<bool>& stop, ScreenPoint itemSP) {
    if (!input::motion::bezierMoveTo(stop, itemSP)) return std::nullopt;
    if (!interruptibleSleep(stop, 100ms)) return std::nullopt;
    return readItemClipboard(stop, 500ms);
}

void MapTask::run(std::atomic<bool>& stop, ProgressCallback report) {
    const auto* gw = params_.gameWindow;
    if (!gw || !gw->valid()) { spdlog::warn("map: game window not found"); return; }
    if (!matcher_.valid()) { spdlog::warn("map: invalid or empty affix pattern"); return; }

    const auto& m  = params_.map;
    const auto& co = params_.coords;

    // Validate required orbs per mode so we fail loudly rather than
    // silently looping over a 0,0 screen point.
    if (m.mode == MapMode::AlchAndScour) {
        if (isUnset(co.orb1) || isUnset(co.orb2)) {
            spdlog::warn("map: AlchAndScour needs orb1 (alch) and orb2 (scour) — set both first");
            return;
        }
    } else if (isUnset(co.orb1)) {
        spdlog::warn("map: Chaos mode needs orb1 (chaos) — set it first");
        return;
    }

    const int totalItems = m.batch ? (m.cols * m.rows) : 1;
    const ScreenPoint orb1SP = gw->clientToScreen(co.orb1);
    const ScreenPoint orb2SP = gw->clientToScreen(co.orb2);
    int orb1Remain = -1;
    int orb2Remain = -1;
    if (auto q = readOrbStack(stop, orb1SP, 500ms)) {
        orb1Remain = *q;
        spdlog::info("map: orb1 stack detected: {}", orb1Remain);
    } else {
        spdlog::warn("map: orb1 stack read failed, using text-diff depletion detection");
    }
    if (m.mode == MapMode::AlchAndScour) {
        if (auto q = readOrbStack(stop, orb2SP, 500ms)) {
            orb2Remain = *q;
            spdlog::info("map: orb2 stack detected: {}", orb2Remain);
        } else {
            spdlog::warn("map: orb2 stack read failed, using text-diff depletion detection");
        }
    }

    TaskProgress prog;
    prog.totalItems = totalItems;

    spdlog::info("map: starting (mode={}, batch={}, items={}, pattern='{}')",
                 static_cast<int>(m.mode), m.batch, totalItems, m.affixes);

    // Publish the full clipboard text + match ranges on hit so the log panel
    // can render matched affixes highlighted. Shared between the alch/scour
    // and chaos branches.
    auto pushHitEntry = [this](const std::string& text) {
        auto matches = matcher_.findAll(text);
        poebot::log::LogEntry e;
        e.level = static_cast<int>(spdlog::level::info);
        e.text  = text;
        e.highlights.reserve(matches.size());
        for (const auto& mm : matches) {
            e.highlights.push_back({mm.offset, mm.length});
        }
        poebot::log::push(std::move(e));
    };

    for (int idx = 0; idx < totalItems && !stop.load(); ++idx) {
        const int row = m.batch ? (idx / m.cols) : 0;
        const int col = m.batch ? (idx % m.cols) : 0;

        const ClientPoint itemCP = grid::itemAt(co.baseItem, co.p01Item, co.p10Item, row, col);
        const ScreenPoint itemSP = gw->clientToScreen(itemCP);

        prog.currentItem = idx + 1;

        // Seed the baseline text. Empty-orb detection works by comparing item
        // text before and after each apply — if the apply was a no-op, the
        // currency pile is empty and we abort the whole task.
        auto prev = readItem(stop, itemSP);
        if (!prev) continue;  // couldn't read — move to next item

        while (!stop.load()) {
            if (!gw->valid()) { spdlog::warn("map: game window lost"); return; }

            if (m.mode == MapMode::AlchAndScour) {
                // --- Step 1: Alchemy --------------------------------------
                if (orb1Remain == 0) {
                    spdlog::warn("map: alch (orb1) depleted by stack count — stopping task");
                    spdlog::info("map: done — ops={}, hits={}", prog.ops, prog.hits);
                    return;
                }
                if (!applyOrb(stop, orb1SP, itemSP)) return;
                if (orb1Remain > 0) --orb1Remain;
                prog.ops++;
                report(prog);

                auto cur = readItem(stop, itemSP);
                if (!cur) continue;

                if (*cur == *prev) {
                    spdlog::warn("map: alch (orb1) appears empty — stopping task");
                    spdlog::info("map: done — ops={}, hits={}", prog.ops, prog.hits);
                    return;
                }
                prev = cur;

                if (matcher_.matches(*cur)) {
                    prog.hits++;
                    report(prog);
                    spdlog::info("map: HIT on ({},{}) after {} ops", row, col, prog.ops);
                    pushHitEntry(*cur);
                    break;
                }

                // --- Step 2: Scour to reset back to white -----------------
                if (orb2Remain == 0) {
                    spdlog::warn("map: scour (orb2) depleted by stack count — stopping task");
                    spdlog::info("map: done — ops={}, hits={}", prog.ops, prog.hits);
                    return;
                }
                if (!applyOrb(stop, orb2SP, itemSP)) return;
                if (orb2Remain > 0) --orb2Remain;
                if (!interruptibleSleep(stop, 100ms)) return;

                auto afterScour = readItem(stop, itemSP);
                if (!afterScour) continue;

                if (*afterScour == *prev) {
                    spdlog::warn("map: scour (orb2) appears empty — stopping task");
                    spdlog::info("map: done — ops={}, hits={}", prog.ops, prog.hits);
                    return;
                }
                prev = afterScour;
                // Loop back to the next alch.

            } else {
                // --- Chaos mode -------------------------------------------
                if (orb1Remain == 0) {
                    spdlog::warn("map: chaos (orb1) depleted by stack count — stopping task");
                    spdlog::info("map: done — ops={}, hits={}", prog.ops, prog.hits);
                    return;
                }
                if (!applyOrb(stop, orb1SP, itemSP)) return;
                if (orb1Remain > 0) --orb1Remain;
                prog.ops++;
                report(prog);

                auto cur = readItem(stop, itemSP);
                if (!cur) continue;

                if (*cur == *prev) {
                    spdlog::warn("map: chaos (orb1) appears empty — stopping task");
                    spdlog::info("map: done — ops={}, hits={}", prog.ops, prog.hits);
                    return;
                }
                prev = cur;

                if (matcher_.matches(*cur)) {
                    prog.hits++;
                    report(prog);
                    spdlog::info("map: HIT on ({},{}) after {} ops", row, col, prog.ops);
                    pushHitEntry(*cur);
                    break;
                }
            }
        }
    }

    spdlog::info("map: done — ops={}, hits={}", prog.ops, prog.hits);
}

}  // namespace poebot::task
