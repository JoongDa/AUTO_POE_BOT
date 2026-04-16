#pragma once
#include <poebot/task/task.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/item/affix_matcher.hpp>
#include <poebot/win/window.hpp>

namespace poebot::task {

// Rolls maps until the affix regex matches. Supports two modes:
//   AlchAndScour — orb1 (Alchemy) then orb2 (Scour) on failure
//   Chaos        — orb1 (Chaos) alone, re-roll on failure
// In batch mode iterates (cols x rows); otherwise rolls only baseItem.
class MapTask : public Task {
public:
    struct Params {
        const poebot::win::GameWindow*  gameWindow = nullptr;
        poebot::config::MapSettings     map;
        poebot::config::ProfileCoords   coords;
    };

    explicit MapTask(Params p);
    const char* name() const override { return "Map"; }
    void run(std::atomic<bool>& stopRequested, ProgressCallback onProgress) override;

private:
    Params                    params_;
    poebot::item::AffixMatcher matcher_;

    // Apply one orb to the item.  Returns false if stop was requested.
    bool applyOrb(std::atomic<bool>& stop, ScreenPoint orbSP, ScreenPoint itemSP);
    // Hover + Ctrl+C + read clipboard.  Returns nullopt on stop/timeout.
    std::optional<std::string> readItem(std::atomic<bool>& stop, ScreenPoint itemSP);
};

}  // namespace poebot::task
