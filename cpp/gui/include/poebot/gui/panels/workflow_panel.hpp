#pragma once
#include <poebot/flow/graph.hpp>
#include <poebot/gui/panel.hpp>
#include <poebot/i18n/i18n.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace poebot::gui::panels {

// Minimal workflow runner — the P0 test entry point. Lists the *.json
// workflows in the active profile's workflows/ dir, loads one, and runs it on
// the shared TaskRunner. A "Generate sample" button writes an example graph so
// there's something to run before the node editor (P3) exists.
//
// The ComfyUI-style canvas editor will replace this panel's body later; the
// file list + run controls become its footer.
class WorkflowPanel : public Panel {
public:
    WorkflowPanel() = default;
    ~WorkflowPanel() override;   // destroys the node-editor context
    WorkflowPanel(const WorkflowPanel&)            = delete;
    WorkflowPanel& operator=(const WorkflowPanel&) = delete;

    const char* name()  const override { return "Workflow"; }
    const char* label() const override { return poebot::i18n::tr("panel.workflow"); }
    void        render(PanelContext& ctx) override;

private:
    // Rescan `dir` for *.json and repopulate files_. Cheap; called on profile
    // switch, after writing the sample, and from the Refresh button.
    void refreshList(const std::filesystem::path& dir);

    // Render graph_ onto the node-editor canvas (nodes + pins + links).
    void drawCanvas();

    std::vector<std::string> files_;        // workflow basenames (no extension)
    int                      selected_ = -1;
    std::string              status_;       // last action result, shown inline
    std::string              lastDir_;      // detects profile/dir change

    // ax::NodeEditor::EditorContext* — kept as void* so this header stays free
    // of imgui_node_editor.h. Created lazily on first render, destroyed in dtor.
    void*                    editorCtx_ = nullptr;

    // The workflow currently shown on the canvas (loaded via "Load to canvas").
    poebot::flow::WorkflowGraph graph_;
    bool                        graphLoaded_ = false;
    bool                        needLayout_  = false;  // push node x/y next frame
    std::string                 loadedName_;           // basename loaded into graph_
};

}  // namespace poebot::gui::panels
