#pragma once
#include <poebot/config/settings.hpp>
#include <poebot/coords.hpp>
#include <poebot/win/window.hpp>

#include <chrono>
#include <string>
#include <string_view>

namespace poebot::gui {

// Coordinate-capture workflow.
//
// Usage: user presses the per-field hotkey (Alt+1, Alt+2, ...) or clicks the
// in-panel button. CaptureService enters a 3-second countdown; the user
// Alt-tabs to the game and positions the cursor. When the deadline expires
// the next tick() reads the cursor, resolves the active profile's ClientPoint
// by name, writes the value, and logs the result.
//
// Resolving by name (not by pointer) means profile switches / reset-to-
// defaults cannot leave the service holding a dangling pointer.
class CaptureService {
public:
    void setGameWindow(const poebot::win::GameWindow* gw) noexcept { gw_ = gw; }
    void setSettings(poebot::config::Settings* s)         noexcept { settings_ = s; }

    // Begin a countdown. `name` is one of the fixed coord names ("orb1"…).
    // Returns false if another capture is already pending or the name is
    // unknown.
    bool startCapture(std::string_view name,
                      std::chrono::milliseconds delay = std::chrono::seconds(3));

    void cancel() noexcept;

    // Called once per UI frame. If a countdown has expired, this performs
    // the capture and returns true (caller should mark settings dirty).
    // Returns false while a countdown is still running or none is pending.
    bool tick();

    // --- UI state queries ---
    bool             active()     const noexcept { return !name_.empty(); }
    std::string_view activeName() const noexcept { return name_; }

    // Seconds remaining on the countdown (0 if not active).
    float remainingSeconds() const noexcept;

private:
    const poebot::win::GameWindow*        gw_        = nullptr;
    poebot::config::Settings*             settings_  = nullptr;
    std::string                           name_;
    std::chrono::steady_clock::time_point deadline_{};
};

}  // namespace poebot::gui
