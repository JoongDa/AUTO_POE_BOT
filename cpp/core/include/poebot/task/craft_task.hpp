#pragma once
#include <poebot/task/task.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/item/affix_matcher.hpp>
#include <poebot/win/window.hpp>

namespace poebot::task {

// Repeatedly applies currency (orb1) to items in a grid until the affix
// regex matches. In batch mode iterates (cols x rows); otherwise crafts
// only the item at baseItem.
class CraftTask : public Task {
public:
    struct Params {
        const poebot::win::GameWindow*  gameWindow = nullptr;
        poebot::config::CraftSettings   craft;
        poebot::config::ProfileCoords   coords;
    };

    explicit CraftTask(Params p);
    const char* name() const override { return "Craft"; }
    void run(std::atomic<bool>& stopRequested, ProgressCallback onProgress) override;

private:
    Params                    params_;
    poebot::item::AffixMatcher matcher_;
};

}  // namespace poebot::task
