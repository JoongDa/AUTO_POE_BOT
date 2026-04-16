#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace poebot::task {

// Snapshot of a running task's progress. Written by the worker thread,
// read by the UI thread — always behind a mutex in TaskRunner.
struct TaskProgress {
    int ops         = 0;
    int hits        = 0;
    int totalItems  = 0;
    int currentItem = 0;
};

using ProgressCallback = std::function<void(const TaskProgress&)>;

// Abstract base for all automation tasks. run() is invoked on a worker
// thread; implementations must check `stopRequested` at every safe point.
class Task {
public:
    virtual ~Task() = default;
    virtual const char* name() const = 0;
    virtual void run(std::atomic<bool>& stopRequested, ProgressCallback onProgress) = 0;
};

// Sleep for `duration` while checking `stopRequested` every ~50 ms so
// cancellation stays responsive.  Returns true if the full sleep elapsed
// (i.e. stop was NOT requested).
inline bool interruptibleSleep(std::atomic<bool>& stop, std::chrono::milliseconds duration) {
    constexpr auto kPoll = std::chrono::milliseconds(50);
    const auto end = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < end) {
        if (stop.load(std::memory_order_relaxed)) return false;
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::steady_clock::now());
        std::this_thread::sleep_for(std::min(remaining, kPoll));
    }
    return !stop.load(std::memory_order_relaxed);
}

}  // namespace poebot::task
