#pragma once
#include <poebot/task/task.hpp>

#include <memory>
#include <mutex>
#include <thread>

namespace poebot::task {

enum class RunnerState { Idle, Running, Stopping, Finished };

// Owns a worker thread and a single Task. Only one task can be active at a
// time. The UI thread calls start/requestStop, the worker thread runs
// Task::run(). progress() is safe to poll from any thread.
class TaskRunner {
public:
    TaskRunner() = default;
    ~TaskRunner();

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    // Launch `task` on a new thread. Returns false if a task is already
    // running. Automatically joins a previously Finished task.
    bool start(std::unique_ptr<Task> task);

    // Non-blocking: sets the stop flag so the worker exits at the next
    // interruptibleSleep / check point.
    void requestStop();

    // Blocks until the worker thread has joined. Transitions to Idle.
    void join();

    RunnerState  state()    const;
    TaskProgress progress() const;
    const char*  taskName() const;

private:
    void workerMain();

    std::unique_ptr<Task>    task_;
    std::thread              worker_;
    std::atomic<RunnerState> state_{RunnerState::Idle};
    std::atomic<bool>        stopRequested_{false};

    mutable std::mutex       mu_;
    TaskProgress             progress_{};
};

}  // namespace poebot::task
