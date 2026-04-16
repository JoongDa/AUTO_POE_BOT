#pragma once
#include <poebot/config/settings.hpp>
#include <poebot/log/log_sink.hpp>

namespace poebot::gui {

// Distinguishes panels that live in the top tab bar from the log strip along
// the bottom. Phase 3 may add more layout zones.
enum class PanelKind {
    Tab,
    Log,
};

// Shared state panels can read/write during render().
struct PanelContext {
    poebot::config::Settings* settings = nullptr;
    poebot::log::ImGuiSink*   logSink  = nullptr;

    // Panels set `dirty = true` when they have committed an edit to settings.
    // App::run() observes this and persists to disk (throttled).
    bool dirty = false;
};

// Abstract base for every UI panel. Keep render() cheap; it runs every frame.
class Panel {
public:
    virtual ~Panel() = default;

    virtual const char* name() const = 0;
    virtual PanelKind   kind() const { return PanelKind::Tab; }

    virtual void render(PanelContext& ctx) = 0;
};

}  // namespace poebot::gui
