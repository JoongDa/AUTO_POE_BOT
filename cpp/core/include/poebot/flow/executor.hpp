#pragma once
#include <poebot/config/profile.hpp>
#include <poebot/flow/graph.hpp>
#include <poebot/task/task.hpp>

#include <atomic>
#include <optional>
#include <string>

namespace poebot::win { class GameWindow; }

// Workflow executor — interprets a WorkflowGraph on a worker thread.
//
// Execution is CONTROL FLOW, not dataflow: edges mean "run this next", and the
// interpreter walks them from the Start node (no topological sort, so loops
// are legal). Data that must cross nodes (clipboard text, last match result)
// rides on the shared FlowContext blackboard rather than on wires.
namespace poebot::flow {

// Shared blackboard + execution dependencies. Node execute functions read and
// write this as they run. Lives on the worker thread for the duration of one
// run(); never touched by the UI thread.
struct FlowContext {
    std::atomic<bool>*                 stop       = nullptr;
    const poebot::win::GameWindow*     gameWindow = nullptr;
    const poebot::config::GameProfile* profile    = nullptr;  // coord resolution
    poebot::task::ProgressCallback     report;

    // --- blackboard ---
    std::string         lastClipboard;  // set by copy_item, read by match
    std::optional<bool> lastMatch;      // set by match, read by future branch
};

// A Task that runs one workflow graph. Follows the project's task convention:
// all inputs are snapshotted at construction (graph + profile copied) so the
// UI thread can keep editing the live workflow while this runs.
class WorkflowTask : public poebot::task::Task {
public:
    struct Params {
        WorkflowGraph                  graph;
        const poebot::win::GameWindow* gameWindow = nullptr;
        poebot::config::GameProfile    profile;  // snapshot for coord lookups
    };

    explicit WorkflowTask(Params p);

    const char* name() const override { return "Workflow"; }
    void run(std::atomic<bool>& stopRequested,
             poebot::task::ProgressCallback onProgress) override;

private:
    Params params_;
};

}  // namespace poebot::flow
