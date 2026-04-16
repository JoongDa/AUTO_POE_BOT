#pragma once
#include <poebot/task/task.hpp>

#include <poebot/config/profile.hpp>
#include <poebot/win/window.hpp>

namespace poebot::task {

// Shift-clicks every cell in the inventory grid to deposit items into the
// open stash tab.  Copies all required data at construction time so it is
// safe to run on a worker thread while the main thread continues rendering.
class DepositTask : public Task {
public:
    struct Params {
        const poebot::win::GameWindow* gameWindow = nullptr;
        poebot::config::DepositSettings deposit;
        poebot::config::ProfileCoords   coords;
    };

    explicit DepositTask(Params p) : params_(std::move(p)) {}

    const char* name() const override { return "Deposit"; }
    void run(std::atomic<bool>& stopRequested, ProgressCallback onProgress) override;

private:
    Params params_;
};

}  // namespace poebot::task
