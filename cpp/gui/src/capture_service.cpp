#include <poebot/gui/capture_service.hpp>

#include <poebot/input/cursor.hpp>

#include <spdlog/spdlog.h>

namespace poebot::gui {

void CaptureService::arm(poebot::ClientPoint* target, std::string label) {
    target_ = target;
    label_  = std::move(label);
    spdlog::info("capture armed: '{}' — press F8 over the game window", label_);
}

void CaptureService::cancel() noexcept {
    if (target_) {
        spdlog::info("capture cancelled: '{}'", label_);
    }
    target_ = nullptr;
    label_.clear();
}

bool CaptureService::fire() {
    if (!target_) return false;

    if (!gw_ || !gw_->valid()) {
        spdlog::warn("capture failed: game window not found — make sure the game is running");
        cancel();
        return false;
    }

    const ScreenPoint sp = poebot::input::cursorPosition();
    const ClientPoint cp = gw_->screenToClient(sp);

    spdlog::info("captured '{}': screen({},{}) → client({},{})",
                 label_, sp.x, sp.y, cp.x, cp.y);

    *target_ = cp;
    target_ = nullptr;
    label_.clear();
    return true;
}

}  // namespace poebot::gui
