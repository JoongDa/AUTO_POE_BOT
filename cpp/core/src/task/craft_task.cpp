#include <poebot/task/craft_task.hpp>

#include <poebot/coords.hpp>
#include <poebot/grid/grid.hpp>
#include <poebot/input/motion.hpp>
#include <poebot/input/mouse.hpp>
#include <poebot/log/log_sink.hpp>
#include <poebot/task/task_util.hpp>

#include <spdlog/spdlog.h>

#include <vector>

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

    // Collect configured orb slots. Unset (0,0) slots are treated as "not
    // present" so a user who only set orb1+orb2 rotates between those two.
    struct OrbSlot {
        const char* label;
        ScreenPoint sp;
        int         remaining = -1;  // -1 => unknown
        bool        depleted  = false;
    };
    std::vector<OrbSlot> orbs;
    auto addOrb = [&](const char* lbl, const ClientPoint& cp) {
        if (!isUnset(cp)) {
            OrbSlot o;
            o.label = lbl;
            o.sp = gw->clientToScreen(cp);
            if (auto qty = readOrbStack(stop, o.sp, 500ms)) {
                o.remaining = *qty;
                o.depleted = (*qty == 0);
                spdlog::info("craft: {} stack detected: {}", lbl, *qty);
            } else {
                spdlog::warn("craft: {} stack read failed, falling back to text-diff depletion detection", lbl);
            }
            orbs.push_back(o);
        }
    };
    addOrb("orb1", co.orb1);
    addOrb("orb2", co.orb2);
    addOrb("orb3", co.orb3);
    if (orbs.empty()) {
        spdlog::warn("craft: no orb slots configured — set orb1/orb2/orb3 first");
        return;
    }

    // Surface the run mode up-front so the log makes it obvious whether
    // we're relying on stack-count termination or the slower text-diff
    // fallback for every orb. Catches "tooltip locale isn't English and
    // hasn't been added to parseStackSize" without the user having to
    // notice three separate "stack read failed" lines above.
    bool anyDetected = false;
    for (const auto& o : orbs) {
        if (o.remaining >= 0) { anyDetected = true; break; }
    }
    if (!anyDetected) {
        spdlog::warn("craft: stack-count detection unavailable for all orbs — "
                     "running in text-diff fallback mode (slower, less precise)");
    }

    const int totalItems = c.batch ? (c.cols * c.rows) : 1;

    TaskProgress prog;
    prog.totalItems = totalItems;

    spdlog::info("craft: starting (batch={}, items={}, orbs={}, pattern='{}')",
                 c.batch, totalItems, orbs.size(), c.affixes);

    // Rotate orb index across items so usage stays roughly even even when an
    // item hits early and the loop moves on before cycling through all orbs.
    size_t orbIdx = 0;

    auto nextOrb = [&]() -> OrbSlot* {
        if (orbs.empty()) return nullptr;
        for (size_t tries = 0; tries < orbs.size(); ++tries) {
            OrbSlot& o = orbs[orbIdx];
            orbIdx = (orbIdx + 1) % orbs.size();
            if (!o.depleted) return &o;
        }
        return nullptr;
    };

    for (int idx = 0; idx < totalItems && !stop.load(); ++idx) {
        const int row = c.batch ? (idx / c.cols) : 0;
        const int col = c.batch ? (idx % c.cols) : 0;

        const ClientPoint itemCP = grid::itemAt(co.baseItem, co.p01Item, co.p10Item, row, col);
        const ScreenPoint itemSP = gw->clientToScreen(itemCP);

        prog.currentItem = idx + 1;

        // Seed the "previous" item text. If an orb is empty, the item won't
        // change across an apply, and we detect the empty pile by comparing
        // before/after text. Without this first read we'd have nothing to
        // compare the first apply against.
        if (!input::motion::bezierMoveTo(stop, itemSP)) return;
        if (!interruptibleSleep(stop, 100ms)) return;
        auto prev = readItemClipboard(stop, 500ms);
        if (!prev) continue;  // couldn't read — move to next item

        // Roll this item until affixes match (or an orb runs out, which stops
        // the whole task per spec).
        while (!stop.load()) {
            if (!gw->valid()) { spdlog::warn("craft: game window lost"); return; }

            OrbSlot* orb = nextOrb();
            if (!orb) {
                spdlog::warn("craft: all configured orb stacks are depleted — stopping task");
                spdlog::info("craft: done — ops={}, hits={}", prog.ops, prog.hits);
                return;
            }

            // Pick up currency.
            if (!input::mouse::humanClick(stop, orb->sp, input::mouse::Button::Right)) return;

            // Apply to item.
            if (!input::mouse::humanClick(stop, itemSP, input::mouse::Button::Left)) return;
            if (!input::motion::gaussSleep(stop, 200ms, 40ms, 120ms)) return;

            if (orb->remaining > 0) {
                --orb->remaining;
                if (orb->remaining == 0) {
                    orb->depleted = true;
                    spdlog::info("craft: {} reached 0 stack after apply", orb->label);
                }
            }

            prog.ops++;
            report(prog);

            // Hover + read item text.
            if (!input::motion::bezierMoveTo(stop, itemSP)) return;
            if (!interruptibleSleep(stop, 100ms)) return;

            auto cur = readItemClipboard(stop, 500ms);
            if (!cur) continue;  // clipboard timeout — retry same orb next

            // Unchanged text means the apply was a no-op: almost always the
            // orb pile is empty. Stop the whole task so the user can refill,
            // even if the other orbs still have stock (matches user spec).
            if (*cur == *prev) {
                spdlog::warn("craft: {} appears empty (item unchanged after apply) — stopping task",
                             orb->label);
                spdlog::info("craft: done — ops={}, hits={}", prog.ops, prog.hits);
                return;
            }
            prev = cur;

            if (matcher_.matches(*cur)) {
                prog.hits++;
                report(prog);
                spdlog::info("craft: HIT on ({},{}) after {} ops (via {})",
                             row, col, prog.ops, orb->label);

                // Dump the full clipboard text with match ranges so the log
                // panel can render it with matched affixes highlighted.
                auto matches = matcher_.findAll(*cur);
                poebot::log::LogEntry e;
                e.level = static_cast<int>(spdlog::level::info);
                e.text  = *cur;
                e.highlights.reserve(matches.size());
                for (const auto& mm : matches) {
                    e.highlights.push_back({mm.offset, mm.length});
                }
                poebot::log::push(std::move(e));
                break;
            }
        }
    }

    spdlog::info("craft: done — ops={}, hits={}", prog.ops, prog.hits);
}

}  // namespace poebot::task
