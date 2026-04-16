#include <poebot/task/task_runner.hpp>

#include <spdlog/spdlog.h>

namespace poebot::task {

TaskRunner::~TaskRunner() {
    requestStop();
    join();
}

bool TaskRunner::start(std::unique_ptr<Task> task) {
    if (!task) return false;

    // Auto-join a finished-but-not-yet-joined task.
    auto s = state_.load();
    if (s == RunnerState::Finished) {
        if (worker_.joinable()) worker_.join();
        state_.store(RunnerState::Idle);
        s = RunnerState::Idle;
    }
    if (s != RunnerState::Idle) return false;

    task_ = std::move(task);
    stopRequested_.store(false);
    state_.store(RunnerState::Running);

    {
        std::lock_guard lock(mu_);
        progress_ = {};
    }

    spdlog::info("task '{}' starting", task_->name());
    worker_ = std::thread(&TaskRunner::workerMain, this);
    return true;
}

void TaskRunner::requestStop() {
    auto s = state_.load();
    if (s == RunnerState::Running) {
        state_.store(RunnerState::Stopping);
        stopRequested_.store(true);
        spdlog::info("task stop requested");
    }
}

void TaskRunner::join() {
    if (worker_.joinable()) worker_.join();
    state_.store(RunnerState::Idle);
    task_.reset();
}

RunnerState TaskRunner::state() const {
    return state_.load();
}

TaskProgress TaskRunner::progress() const {
    std::lock_guard lock(mu_);
    return progress_;
}

const char* TaskRunner::taskName() const {
    return task_ ? task_->name() : "";
}

void TaskRunner::workerMain() {
    try {
        task_->run(stopRequested_, [this](const TaskProgress& p) {
            std::lock_guard lock(mu_);
            progress_ = p;
        });
    } catch (const std::exception& e) {
        spdlog::error("task '{}' threw: {}", task_->name(), e.what());
    }
    state_.store(RunnerState::Finished);
    spdlog::info("task '{}' finished (ops={}, hits={})",
                 task_->name(), progress_.ops, progress_.hits);
}

}  // namespace poebot::task
