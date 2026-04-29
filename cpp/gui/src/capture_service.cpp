#include <poebot/gui/capture_service.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/input/cursor.hpp>
#include <poebot/task/task_util.hpp>

#include <spdlog/spdlog.h>

namespace poebot::gui {

bool CaptureService::startCapture(std::string_view name,
                                  std::chrono::milliseconds delay) {
    if (active()) {
        spdlog::warn("capture: already recording '{}', ignoring request for '{}'",
                     name_, name);
        return false;
    }
    if (!settings_) {
        spdlog::error("capture: no Settings bound");
        return false;
    }
    auto* prof = settings_->active();
    if (!prof || !poebot::config::findCoordByName(*prof, name)) {
        spdlog::warn("capture: unknown coord name '{}'", name);
        return false;
    }

    name_     = std::string(name);
    deadline_ = std::chrono::steady_clock::now() + delay;

    spdlog::info("capture: recording '{}' in {}s — switch to the game window now",
                 name_,
                 std::chrono::duration_cast<std::chrono::seconds>(delay).count());
    return true;
}

void CaptureService::cancel() noexcept {
    if (!name_.empty()) {
        spdlog::info("capture: cancelled '{}'", name_);
    }
    name_.clear();
    deadline_ = {};
}

bool CaptureService::captureNow(std::string_view name) {
    // Snapshot the cursor and write the coord right now. Any in-flight
    // countdown is abandoned (its target may differ from `name`).
    if (active()) {
        name_.clear();
        deadline_ = {};
    }
    if (!gw_ || !gw_->valid()) {
        spdlog::warn("capture: game window not found — '{}' not recorded", name);
        return false;
    }
    if (!settings_) {
        spdlog::error("capture: no Settings bound");
        return false;
    }
    auto* prof = settings_->active();
    if (!prof) return false;
    auto* target = poebot::config::findCoordByName(*prof, name);
    if (!target) {
        spdlog::warn("capture: unknown coord name '{}'", name);
        return false;
    }

    const ScreenPoint sp = poebot::input::cursorPosition();
    const ClientPoint cp = gw_->screenToClient(sp);
    *target = cp;

    // For currency anchors, read and cache the current stack size so the
    // Coordinates tab can display it next to (x,y), matching the AHK UX.
    auto setOrbQty = [&](int* slot) {
        if (!slot) return;
        std::atomic<bool> stop{false};
        if (auto qty = poebot::task::readOrbStack(stop, sp, std::chrono::milliseconds(500))) {
            *slot = *qty;
        } else {
            *slot = -1;
        }
    };
    if (name == "orb1") setOrbQty(&prof->coords.orb1Qty);
    if (name == "orb2") setOrbQty(&prof->coords.orb2Qty);
    if (name == "orb3") setOrbQty(&prof->coords.orb3Qty);

    spdlog::info("capture: '{}' recorded screen({},{}) → client({},{})",
                 name, sp.x, sp.y, cp.x, cp.y);
    return true;
}

bool CaptureService::tick() {
    if (!active()) return false;
    if (std::chrono::steady_clock::now() < deadline_) return false;

    // Deadline reached — take the snapshot. Copy the name first because
    // captureNow clears our state.
    const std::string pending = name_;
    name_.clear();
    deadline_ = {};
    return captureNow(pending);
}

float CaptureService::remainingSeconds() const noexcept {
    if (!active()) return 0.0f;
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline_) return 0.0f;
    const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now).count();
    return static_cast<float>(diff) / 1000.0f;
}

}  // namespace poebot::gui
