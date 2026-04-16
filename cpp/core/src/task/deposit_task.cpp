#include <poebot/task/deposit_task.hpp>

#include <poebot/grid/grid.hpp>
#include <poebot/input/mouse.hpp>

#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

namespace poebot::task {

void DepositTask::run(std::atomic<bool>& stop, ProgressCallback report) {
    const auto& d  = params_.deposit;
    const auto& c  = params_.coords;
    const auto* gw = params_.gameWindow;

    if (!gw || !gw->valid()) {
        spdlog::warn("deposit: game window not found");
        return;
    }

    const int total = d.cols * d.rows;
    TaskProgress prog;
    prog.totalItems = total;

    spdlog::info("deposit: starting {}x{} grid ({} cells)", d.cols, d.rows, total);

    for (int row = 0; row < d.rows; ++row) {
        for (int col = 0; col < d.cols; ++col) {
            if (stop.load(std::memory_order_relaxed)) return;

            // Verify the game window is still alive.
            if (!gw->valid()) {
                spdlog::warn("deposit: game window lost mid-task");
                return;
            }

            const ClientPoint cp = grid::invAt(c.invBase, c.invP01, c.invP10, row, col);
            const ScreenPoint sp = gw->clientToScreen(cp);

            input::mouse::moveTo(sp);
            if (!interruptibleSleep(stop, 50ms)) return;

            input::mouse::shiftClick();
            if (!interruptibleSleep(stop, 100ms)) return;

            prog.ops++;
            prog.currentItem = row * d.cols + col + 1;
            report(prog);
        }
    }

    spdlog::info("deposit: completed {} cells", total);
}

}  // namespace poebot::task
