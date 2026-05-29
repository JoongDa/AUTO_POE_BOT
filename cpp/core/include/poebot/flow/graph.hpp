#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// Workflow graph data model — the in-memory representation of a user-authored
// automation flow (ComfyUI-style canvas, control-flow semantics).
//
// This header is intentionally pure data: no JSON, no UI, no execution logic.
// Serialization lives in flow/serialize.hpp; the node registry that gives each
// `type` meaning lives in flow/node_registry.hpp (added in a later step);
// execution lives in the WorkflowTask interpreter.
namespace poebot::flow {

// One node instance placed on the canvas. `type` keys into the node registry
// (allNodeTypes()); it is NOT an enum so new node types can be added by
// registration alone, and so an unknown type loaded from an imported file can
// round-trip without data loss. `params` is a flat string→string bag holding
// the per-instance configuration the node type understands (e.g.
// coordRef="orb1", button="right") — kept stringly-typed so the model stays
// agnostic of any particular node's parameter shape.
//
// `x`/`y` are canvas coordinates owned HERE, not in imgui-node-editor's own
// settings file — the editor reads them on load and writes them back on move,
// so a workflow's layout travels with its JSON on export/import.
struct FlowNode {
    int                                          id = 0;
    std::string                                  type;
    std::unordered_map<std::string, std::string> params;
    float                                        x = 0.0f;
    float                                        y = 0.0f;
};

// A directed control-flow edge: "after fromNode finishes via its fromPin,
// continue at toNode's toPin". Pins are integer roles defined by the node
// type — most nodes expose a single output pin (0 = "next"), while a branch
// node like Match exposes 0 = onMatch and 1 = onFail. Input pins are 0 for
// every node in the current model (single entry); the field exists so the
// editor and a future multi-entry node don't need a schema change.
struct FlowEdge {
    int id       = 0;
    int fromNode = 0;
    int fromPin  = 0;
    int toNode   = 0;
    int toPin    = 0;
};

// A complete workflow. `version` drives forward-migration the same way the
// settings schema does. Nodes and edges are plain ordered vectors — lookup by
// id is the executor/editor's concern, not the model's.
struct WorkflowGraph {
    int                   version = 1;
    std::string           name;
    std::vector<FlowNode> nodes;
    std::vector<FlowEdge> edges;
};

}  // namespace poebot::flow
