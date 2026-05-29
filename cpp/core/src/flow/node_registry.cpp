#include <poebot/flow/node_registry.hpp>

namespace poebot::flow {

// The built-in node set. Maps 1:1 to the modules the user listed:
//   物品/模板/基准坐标 -> move_to (one type, different coordRef preset)
//   坐标偏移数          -> move_to_grid (base coord + row/col, uses grid::itemAt)
//   鼠标点击            -> click
//   物品复制            -> copy_item
//   物品校验            -> match (branches: onMatch / onFail)
// Plus Start/End terminals and a Wait helper. Repeat / ForEach come in P2.
//
// labelKey strings are i18n keys; their zh/en translations get added when the
// editor UI lands (P3). Until then tr() falls through to the key text, which
// is fine for headless work.
const std::vector<NodeType>& allNodeTypes() {
    static const std::vector<NodeType> kTypes = {
        {"start", "flow.node.start", NodeCategory::Flow, false,
         {{"flow.pin.next"}},
         {}},

        {"end", "flow.node.end", NodeCategory::Flow, true,
         {},
         {}},

        {"move_to", "flow.node.move_to", NodeCategory::Coord, true,
         {{"flow.pin.next"}},
         {{"coordRef", "flow.param.coord", ParamType::CoordRef, "baseItem", {}}}},

        {"move_to_grid", "flow.node.move_to_grid", NodeCategory::Coord, true,
         {{"flow.pin.next"}},
         {{"baseRef", "flow.param.base", ParamType::CoordRef, "baseItem", {}},
          {"row", "flow.param.row", ParamType::Int, "0", {}},
          {"col", "flow.param.col", ParamType::Int, "0", {}}}},

        {"click", "flow.node.click", NodeCategory::Action, true,
         {{"flow.pin.next"}},
         {{"button", "flow.param.button", ParamType::Enum, "left",
           {"left", "right"}}}},

        {"copy_item", "flow.node.copy_item", NodeCategory::Data, true,
         {{"flow.pin.next"}},
         {}},

        {"match", "flow.node.match", NodeCategory::Data, true,
         {{"flow.pin.onMatch"}, {"flow.pin.onFail"}},
         {{"affix", "flow.param.affix", ParamType::Text, "", {}}}},

        {"wait", "flow.node.wait", NodeCategory::Flow, true,
         {{"flow.pin.next"}},
         {{"ms", "flow.param.ms", ParamType::Int, "200", {}},
          {"jitter", "flow.param.jitter", ParamType::Int, "60", {}}}},
    };
    return kTypes;
}

const NodeType* findNodeType(std::string_view id) {
    for (const auto& t : allNodeTypes()) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

}  // namespace poebot::flow
