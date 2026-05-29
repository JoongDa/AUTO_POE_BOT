#include <poebot/flow/executor.hpp>

#include <poebot/coords.hpp>
#include <poebot/input/motion.hpp>
#include <poebot/input/mouse.hpp>
#include <poebot/item/affix_matcher.hpp>
#include <poebot/task/task_util.hpp>
#include <poebot/win/window.hpp>

#include <spdlog/spdlog.h>

#include <charconv>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

using namespace std::chrono_literals;

namespace poebot::flow {

namespace {

// Safety cap: control-flow graphs may contain intentional loops, so the
// interpreter can't assume termination. Bounds a runaway cycle with no
// reachable End. 100k steps is far beyond any real workflow.
constexpr int kMaxSteps = 100000;

// --- param accessors --------------------------------------------------------

std::string paramStr(const FlowNode& n, const char* key) {
    auto it = n.params.find(key);
    return it == n.params.end() ? std::string{} : it->second;
}

int paramInt(const FlowNode& n, const char* key, int def) {
    auto it = n.params.find(key);
    if (it == n.params.end()) return def;
    const std::string& s = it->second;
    int v = def;
    const auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    return res.ec == std::errc{} ? v : def;
}

// Resolve a coordRef to a client-space point: first the profile's fixed coords
// (orb1/baseItem/…), then the calibrated pool (chaos_1/…). Returns nullopt when
// the name is unknown or the slot is still unset (0,0).
std::optional<ClientPoint> resolveCoord(const FlowContext& ctx, const std::string& ref) {
    if (ref.empty() || !ctx.profile) return std::nullopt;
    if (const ClientPoint* p = config::findCoordByName(*ctx.profile, ref)) {
        if (!isUnset(*p)) return *p;
    }
    if (auto it = ctx.profile->calibrated.find(ref); it != ctx.profile->calibrated.end()) {
        if (!isUnset(it->second.pos)) return it->second.pos;
    }
    return std::nullopt;
}

// --- node handlers ----------------------------------------------------------
// Each returns the output-pin index to follow (0 = "next" for single-outlet
// nodes; Match returns 0=onMatch / 1=onFail).

int execMoveTo(const FlowNode& n, FlowContext& ctx) {
    const std::string ref = paramStr(n, "coordRef");
    const auto cp = resolveCoord(ctx, ref);
    if (!cp) {
        spdlog::warn("flow: move_to '{}' — coord unset/unknown, skipping", ref);
        return 0;
    }
    if (!ctx.gameWindow || !ctx.gameWindow->valid()) {
        spdlog::warn("flow: move_to — game window invalid, skipping");
        return 0;
    }
    const ScreenPoint sp = ctx.gameWindow->clientToScreen(*cp);
    input::motion::bezierMoveTo(*ctx.stop, sp);  // false => stop; loop ends next
    return 0;
}

int execClick(const FlowNode& n, FlowContext& ctx) {
    const std::string btn = paramStr(n, "button");
    const auto button = (btn == "right") ? input::mouse::Button::Right
                                         : input::mouse::Button::Left;
    // Small Gaussian settle before/after so a click in a loop doesn't read as
    // a metronome. Bail before clicking if stop landed during the pre-delay.
    if (!input::motion::gaussSleep(*ctx.stop, 80ms)) return 0;
    input::mouse::click(button);
    input::motion::gaussSleep(*ctx.stop, 120ms);
    return 0;
}

int execCopyItem(const FlowNode&, FlowContext& ctx) {
    if (auto txt = task::readItemClipboard(*ctx.stop)) {
        ctx.lastClipboard = std::move(*txt);
    } else {
        spdlog::warn("flow: copy_item — clipboard read failed/timeout");
    }
    return 0;
}

int execMatch(const FlowNode& n, FlowContext& ctx) {
    const std::string pat = paramStr(n, "affix");
    const item::AffixMatcher m(pat);
    if (!m.valid()) {
        spdlog::warn("flow: match — invalid/empty affix pattern '{}'", pat);
        ctx.lastMatch = false;
        return 1;  // onFail
    }
    const bool hit = m.matches(ctx.lastClipboard);
    ctx.lastMatch = hit;
    spdlog::info("flow: match '{}' -> {}", pat, hit ? "HIT" : "miss");
    return hit ? 0 : 1;  // 0=onMatch, 1=onFail
}

int execWait(const FlowNode& n, FlowContext& ctx) {
    const int ms     = paramInt(n, "ms", 200);
    const int jitter = paramInt(n, "jitter", 60);
    input::motion::gaussSleep(*ctx.stop,
                              std::chrono::milliseconds(ms),
                              std::chrono::milliseconds(jitter));
    return 0;
}

// Dispatch by node type. Unknown/unimplemented types log and fall through to
// the "next" pin so an imported-but-unsupported node doesn't dead-end a run.
int dispatch(const FlowNode& n, FlowContext& ctx) {
    const std::string& t = n.type;
    if (t == "start")     return 0;
    if (t == "move_to")   return execMoveTo(n, ctx);
    if (t == "click")     return execClick(n, ctx);
    if (t == "copy_item") return execCopyItem(n, ctx);
    if (t == "match")     return execMatch(n, ctx);
    if (t == "wait")      return execWait(n, ctx);
    if (t == "move_to_grid") {
        // Deferred (P0.3b option C): grid anchors still TBD.
        spdlog::info("flow: [stub] move_to_grid not yet implemented, skipping");
        return 0;
    }
    spdlog::warn("flow: unknown node type '{}' — skipping", t);
    return 0;
}

}  // namespace

WorkflowTask::WorkflowTask(Params p) : params_(std::move(p)) {}

void WorkflowTask::run(std::atomic<bool>& stop,
                       poebot::task::ProgressCallback report) {
    const auto& g = params_.graph;

    // Index nodes by id for O(1) edge-following. Duplicate ids keep the first
    // (the editor is responsible for uniqueness; this just stays well-defined).
    std::unordered_map<int, const FlowNode*> byId;
    byId.reserve(g.nodes.size());
    for (const auto& n : g.nodes) byId.emplace(n.id, &n);

    // Entry point.
    const FlowNode* cur = nullptr;
    for (const auto& n : g.nodes) {
        if (n.type == "start") { cur = &n; break; }
    }
    if (!cur) {
        spdlog::warn("flow: no Start node — nothing to run");
        return;
    }

    FlowContext ctx;
    ctx.stop       = &stop;
    ctx.gameWindow = params_.gameWindow;
    ctx.profile    = &params_.profile;
    ctx.report     = report;

    poebot::task::TaskProgress prog;

    int steps = 0;
    while (cur && cur->type != "end" && !stop.load() && steps++ < kMaxSteps) {
        const int outPin = dispatch(*cur, ctx);

        // Follow the edge leaving cur via outPin. No matching edge => the
        // branch is unwired, which ends the run (well-defined stop).
        const FlowNode* next = nullptr;
        for (const auto& e : g.edges) {
            if (e.fromNode == cur->id && e.fromPin == outPin) {
                if (auto it = byId.find(e.toNode); it != byId.end()) next = it->second;
                break;
            }
        }

        prog.ops++;
        report(prog);
        cur = next;
    }

    if (steps >= kMaxSteps) {
        spdlog::warn("flow: hit max step cap ({}) — stopping (loop without exit?)",
                     kMaxSteps);
    }
    spdlog::info("flow: done — steps={}", prog.ops);
}

}  // namespace poebot::flow
