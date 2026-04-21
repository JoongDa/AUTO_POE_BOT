#include <poebot/gui/capture_service.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/input/cursor.hpp>

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

bool CaptureService::tick() {
    if (!active()) return false;
    if (std::chrono::steady_clock::now() < deadline_) return false;

    // Deadline reached — perform the capture.
    if (!gw_ || !gw_->valid()) {
        spdlog::warn("capture: game window not found — '{}' not recorded", name_);
        cancel();
        return false;
    }
    if (!settings_) { cancel(); return false; }
    auto* prof = settings_->active();
    if (!prof) { cancel(); return false; }
    auto* target = poebot::config::findCoordByName(*prof, name_);
    if (!target) { cancel(); return false; }

    const ScreenPoint sp = poebot::input::cursorPosition();
    const ClientPoint cp = gw_->screenToClient(sp);
    *target = cp;

    spdlog::info("capture: '{}' recorded screen({},{}) → client({},{})",
                 name_, sp.x, sp.y, cp.x, cp.y);

    name_.clear();
    deadline_ = {};
    return true;
}

float CaptureService::remainingSeconds() const noexcept {
    if (!active()) return 0.0f;
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline_) return 0.0f;
    const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now).count();
    return static_cast<float>(diff) / 1000.0f;
}

}  // namespace poebot::gui
