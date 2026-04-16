#pragma once
#include <poebot/coords.hpp>
#include <poebot/win/window.hpp>

#include <string>

namespace poebot::gui {

// Coordinates the "Capture" workflow: a panel arms a target ClientPoint*,
// then when the user presses the capture hotkey the service reads the cursor
// position, converts via GameWindow, and writes the result.
class CaptureService {
public:
    void setGameWindow(const poebot::win::GameWindow* gw) noexcept { gw_ = gw; }

    // Arm capture for a specific coord field. `label` is for UI feedback
    // (e.g. "orb1"). Only one capture can be pending at a time.
    void arm(poebot::ClientPoint* target, std::string label);
    void cancel() noexcept;

    bool               armed() const noexcept { return target_ != nullptr; }
    const std::string& label() const noexcept { return label_; }

    // Called to check if a given pointer is the currently armed target. Used
    // by panels to highlight the pending-capture row.
    bool isArmedFor(const poebot::ClientPoint* p) const noexcept { return target_ == p; }

    // Read cursor, convert to client coords, write to *target_, disarm.
    // Returns true if the capture succeeded (caller should set dirty flag).
    // Returns false if no capture was armed, or the game window is invalid.
    bool fire();

private:
    const poebot::win::GameWindow* gw_     = nullptr;
    poebot::ClientPoint*           target_ = nullptr;
    std::string                    label_;
};

}  // namespace poebot::gui
