#include <poebot/gui/panels/workflow_panel.hpp>

#include <poebot/flow/executor.hpp>
#include <poebot/flow/node_registry.hpp>
#include <poebot/flow/serialize.hpp>
#include <poebot/i18n/i18n.hpp>
#include <poebot/task/task_runner.hpp>
#include <poebot/win/window.hpp>

#include <imgui.h>
#include <imgui-node-editor/imgui_node_editor.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>

namespace ed = ax::NodeEditor;

namespace poebot::gui::panels {

namespace {

// A safe, non-destructive example: move to the base item, copy its text, and
// run an affix check. No clicks, so it can't reroll/ruin an item — it just
// exercises move_to + copy_item + match (and both Match outlets) end to end.
poebot::flow::WorkflowGraph sampleGraph() {
    poebot::flow::WorkflowGraph g;
    g.name  = "example";
    g.nodes = {
        {1, "start",     {},                                 40.0f,  120.0f},
        {2, "move_to",   {{"coordRef", "baseItem"}},          220.0f, 120.0f},
        {3, "copy_item", {},                                  400.0f, 120.0f},
        {4, "match",     {{"affix", "Life|Mana|Resist"}},     580.0f, 120.0f},
        {5, "end",       {},                                  780.0f, 120.0f},
    };
    g.edges = {
        {1, 1, 0, 2, 0},   // start    -> move_to
        {2, 2, 0, 3, 0},   // move_to  -> copy_item
        {3, 3, 0, 4, 0},   // copy_item-> match
        {4, 4, 0, 5, 0},   // match onMatch -> end
        {5, 4, 1, 5, 0},   // match onFail  -> end
    };
    return g;
}

// imgui-node-editor needs ids unique across nodes/pins/links. Partition the id
// space by base offset so small model node-ids/edge-ids never collide once
// mapped onto the canvas. (node id < 65536 assumed — far beyond any workflow.)
constexpr uintptr_t kPinBase  = 1ull << 20;
constexpr uintptr_t kLinkBase = 1ull << 21;

ed::NodeId canvasNodeId(int id) { return ed::NodeId(static_cast<uintptr_t>(id)); }
ed::PinId  inputPinId(int node) {
    return ed::PinId(kPinBase + static_cast<uintptr_t>(node) * 16u);
}
ed::PinId  outputPinId(int node, int pin) {
    return ed::PinId(kPinBase + static_cast<uintptr_t>(node) * 16u + 1u
                     + static_cast<uintptr_t>(pin));
}
ed::LinkId canvasLinkId(int edge) {
    return ed::LinkId(kLinkBase + static_cast<uintptr_t>(edge));
}

// "flow.pin.onMatch" -> "onMatch": short pin label for the canvas.
const char* pinShort(const char* labelKey) {
    const std::string_view s(labelKey);
    const auto pos = s.rfind('.');
    return pos == std::string_view::npos ? labelKey : labelKey + pos + 1;
}

}  // namespace

WorkflowPanel::~WorkflowPanel() {
    if (editorCtx_) {
        ed::DestroyEditor(static_cast<ed::EditorContext*>(editorCtx_));
        editorCtx_ = nullptr;
    }
}

void WorkflowPanel::refreshList(const std::filesystem::path& dir) {
    files_.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return;
    for (auto it = std::filesystem::directory_iterator(dir, ec);
         it != std::filesystem::directory_iterator(); ++it) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        const auto& p = it->path();
        if (p.extension() == ".json") files_.push_back(p.stem().string());
    }
    std::sort(files_.begin(), files_.end());
}

void WorkflowPanel::render(PanelContext& ctx) {
    using poebot::i18n::tr;
    using poebot::task::RunnerState;

    if (!ctx.settings || !ctx.settings->active()) {
        ImGui::TextUnformatted(tr("common.no_active_profile"));
        return;
    }
    if (!ctx.settingsRoot) {
        ImGui::TextDisabled("no settings root");
        return;
    }

    const auto* prof = ctx.settings->active();
    const std::filesystem::path dir = *ctx.settingsRoot / prof->name / "workflows";

    // Rescan when the active profile (and thus the dir) changes.
    if (dir.string() != lastDir_) {
        lastDir_ = dir.string();
        selected_ = -1;
        status_.clear();
        refreshList(dir);
    }

    ImGui::TextDisabled("%s", dir.string().c_str());
    ImGui::TextWrapped("Minimal runner (P0 test entry). Pick a workflow and Run. "
                       "The node editor comes in a later phase.");
    ImGui::Spacing();

    if (ImGui::Button("Generate sample")) {
        if (poebot::flow::saveToFile(sampleGraph(), dir / "example.json")) {
            status_ = "wrote example.json";
            refreshList(dir);
        } else {
            status_ = "failed to write example.json";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) refreshList(dir);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (files_.empty()) {
        ImGui::TextDisabled("(no .json workflows — click \"Generate sample\")");
    } else {
        for (int i = 0; i < static_cast<int>(files_.size()); ++i) {
            if (ImGui::Selectable(files_[static_cast<std::size_t>(i)].c_str(),
                                  selected_ == i)) {
                selected_ = i;
            }
        }
    }

    ImGui::Spacing();

    auto* runner = ctx.taskRunner;
    const RunnerState state = runner ? runner->state() : RunnerState::Idle;
    const bool ours = runner && std::string_view(runner->taskName()) == "Workflow";
    const bool idle = (state == RunnerState::Idle);

    // Load the selected workflow onto the canvas.
    ImGui::BeginDisabled(selected_ < 0);
    if (ImGui::Button("Load to canvas")) {
        const auto path = dir / (files_[static_cast<std::size_t>(selected_)] + ".json");
        if (auto g = poebot::flow::loadFromFile(path)) {
            graph_       = std::move(*g);
            graphLoaded_ = true;
            needLayout_  = true;
            loadedName_  = files_[static_cast<std::size_t>(selected_)];
            status_ = "loaded: " + loadedName_ + "  (" +
                      std::to_string(graph_.nodes.size()) + " nodes, " +
                      std::to_string(graph_.edges.size()) + " edges)";
        } else {
            status_ = "load failed (invalid JSON?)";
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();

    // Run the loaded graph. Snapshot-copied so the canvas keeps its own copy.
    ImGui::BeginDisabled(!runner || !idle || !graphLoaded_);
    if (ImGui::Button("Run")) {
        poebot::flow::WorkflowTask::Params p;
        p.graph      = graph_;
        p.gameWindow = ctx.gameWindow;
        p.profile    = *prof;  // snapshot for coord resolution
        runner->start(std::make_unique<poebot::flow::WorkflowTask>(std::move(p)));
        status_ = "started: " + loadedName_;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!(runner && ours && state == RunnerState::Running));
    if (ImGui::Button("Stop")) runner->requestStop();
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (runner && !idle && ours) {
        ImGui::Text("running — steps: %d", runner->progress().ops);
        if (state == RunnerState::Stopping) {
            ImGui::SameLine();
            ImGui::TextDisabled("(stopping...)");
        }
    } else if (runner && !idle) {
        ImGui::TextDisabled("other task running: %s", runner->taskName());
    }
    if (!status_.empty()) ImGui::TextDisabled("%s", status_.c_str());

    if (!ctx.gameWindow || !ctx.gameWindow->valid()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.3f, 1.0f));
        ImGui::TextWrapped("Game window not found — move_to/click steps will be skipped.");
        ImGui::PopStyleColor();
    }

    // --- node-editor canvas (P3.1: render the loaded graph, read-only) -------
    ImGui::Spacing();
    ImGui::Separator();
    drawCanvas();
}

void WorkflowPanel::drawCanvas() {
    // Lazily create the editor context; disable its auto settings file since we
    // own persistence via the per-workflow JSON.
    if (!editorCtx_) {
        ed::Config cfg;
        cfg.SettingsFile = nullptr;
        editorCtx_ = ed::CreateEditor(&cfg);
    }
    ed::SetCurrentEditor(static_cast<ed::EditorContext*>(editorCtx_));
    ed::Begin("##wf_canvas", ImVec2(0.0f, 0.0f));

    if (!graphLoaded_) {
        ed::End();
        ed::SetCurrentEditor(nullptr);
        return;
    }

    // Nodes. On the first frame after a load, push each node to its stored x/y;
    // afterwards leave positions alone so canvas panning/dragging sticks.
    for (const auto& n : graph_.nodes) {
        const auto* type = poebot::flow::findNodeType(n.type);
        if (needLayout_) ed::SetNodePosition(canvasNodeId(n.id), ImVec2(n.x, n.y));

        ed::BeginNode(canvasNodeId(n.id));
        ImGui::TextUnformatted(n.type.c_str());

        const bool hasInput = !type || type->hasInput;  // unknown type: assume input
        if (hasInput) {
            ed::BeginPin(inputPinId(n.id), ed::PinKind::Input);
            ImGui::TextUnformatted("-> in");
            ed::EndPin();
        }

        const int outCount = type ? static_cast<int>(type->outputs.size()) : 1;
        for (int i = 0; i < outCount; ++i) {
            ed::BeginPin(outputPinId(n.id, i), ed::PinKind::Output);
            if (type && i < static_cast<int>(type->outputs.size()))
                ImGui::Text("%s ->", pinShort(type->outputs[static_cast<std::size_t>(i)].labelKey));
            else
                ImGui::TextUnformatted("out ->");
            ed::EndPin();
        }

        for (const auto& [k, v] : n.params) {
            ImGui::TextDisabled("%s = %s", k.c_str(), v.c_str());
        }
        ed::EndNode();
    }

    // Links — one per control-flow edge.
    for (const auto& e : graph_.edges) {
        ed::Link(canvasLinkId(e.id),
                 outputPinId(e.fromNode, e.fromPin),
                 inputPinId(e.toNode));
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);
    needLayout_ = false;
}

}  // namespace poebot::gui::panels
