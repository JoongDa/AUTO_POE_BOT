#pragma once
#include <string>
#include <string_view>
#include <vector>

// Node registry — the single source of truth for "what kinds of nodes exist".
// One entry here surfaces a node type in three places at once (mirrors how
// hotkey::allHotkeyActions() works): the left-hand palette in the editor, the
// JSON (de)serializer, and — once wired in P0.3 — the executor.
//
// This file is metadata only: it declares each node type's pins and parameter
// schema. It deliberately carries NO execution logic yet; FlowNode::params
// (string→string) holds per-instance values keyed by the ParamSpec::key
// entries declared here.
namespace poebot::flow {

// How the editor should render a parameter, and how the executor will later
// interpret its string value in FlowNode::params.
enum class ParamType {
    Enum,      // one of a fixed option set (e.g. button = left|right)
    CoordRef,  // a coordinate reference; options are NOT static — the editor
               // fills them at render time from the active profile (fixed
               // coords like "orb1"/"baseItem" plus calibrated keys like
               // "chaos_1"). Stored as the coord's name string.
    Int,       // integer entered as text, parsed at execution time
    Text,      // free text (e.g. an affix regex for the Match node)
};

// One configurable parameter of a node type. The live value lives in
// FlowNode::params[key]; everything here is the static schema the UI and
// executor share.
struct ParamSpec {
    std::string              key;          // FlowNode::params key
    const char*              labelKey;     // i18n key for the field label
    ParamType                type;
    std::string              defaultValue; // seeded into params on node create
    std::vector<std::string> options;      // Enum only; empty otherwise
};

// One output execution pin. Its index in NodeType::outputs is the value
// stored in FlowEdge::fromPin, so order is significant and stable.
struct PinSpec {
    const char* labelKey;   // i18n key, e.g. "flow.pin.next" / "flow.pin.onMatch"
};

// Palette grouping in the editor's left column.
enum class NodeCategory {
    Flow,     // Start / End / Wait (and later Repeat / ForEach)
    Coord,    // the MoveTo family
    Action,   // Click (and later KeyPress)
    Data,     // CopyItem / Match
};

// Complete declaration of one node type. Aggregate (no ctors) so the registry
// table can be a plain brace-initialized list.
struct NodeType {
    std::string            id;         // stable serialization key, e.g. "move_to"
    const char*            labelKey;   // i18n display name
    NodeCategory           category;
    bool                   hasInput;   // false only for Start
    std::vector<PinSpec>   outputs;    // Start=1, End=0, Match=2, most=1
    std::vector<ParamSpec> params;
};

// The registry. Process-wide singleton, reference semantics.
const std::vector<NodeType>& allNodeTypes();

// Look a type up by its stable id. Returns nullptr for unknown ids so an
// imported workflow referencing a node type we don't have doesn't crash —
// the editor can render it as an "unknown node" placeholder instead.
const NodeType* findNodeType(std::string_view id);

}  // namespace poebot::flow
