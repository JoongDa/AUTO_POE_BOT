#include <poebot/flow/serialize.hpp>

#include "../config/atomic_replace_file.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <system_error>

namespace poebot::flow {

namespace {

using nlohmann::json;

json nodeToJson(const FlowNode& n) {
    json j;
    j["id"]     = n.id;
    j["type"]   = n.type;
    j["params"] = n.params;
    j["x"]      = n.x;
    j["y"]      = n.y;
    return j;
}

json edgeToJson(const FlowEdge& e) {
    json j;
    j["id"]       = e.id;
    j["fromNode"] = e.fromNode;
    j["fromPin"]  = e.fromPin;
    j["toNode"]   = e.toNode;
    j["toPin"]    = e.toPin;
    return j;
}

// `id` + `type` are required (a node with neither can't be referenced by an
// edge nor executed); everything else defaults so older / hand-trimmed files
// still load. A missing required field throws, caught by fromJson → nullopt.
FlowNode nodeFromJson(const json& j) {
    FlowNode n;
    n.id     = j.at("id").get<int>();
    n.type   = j.at("type").get<std::string>();
    n.params = j.value("params", std::unordered_map<std::string, std::string>{});
    n.x      = j.value("x", 0.0f);
    n.y      = j.value("y", 0.0f);
    return n;
}

// `fromNode` + `toNode` are required (an edge that doesn't connect two nodes
// is meaningless); pins and id default to 0.
FlowEdge edgeFromJson(const json& j) {
    FlowEdge e;
    e.id       = j.value("id", 0);
    e.fromNode = j.at("fromNode").get<int>();
    e.fromPin  = j.value("fromPin", 0);
    e.toNode   = j.at("toNode").get<int>();
    e.toPin    = j.value("toPin", 0);
    return e;
}

}  // namespace

json toJson(const WorkflowGraph& g) {
    json j;
    j["version"] = g.version;
    j["name"]    = g.name;
    j["nodes"]   = json::array();
    for (const auto& n : g.nodes) j["nodes"].push_back(nodeToJson(n));
    j["edges"]   = json::array();
    for (const auto& e : g.edges) j["edges"].push_back(edgeToJson(e));
    return j;
}

std::optional<WorkflowGraph> fromJson(const json& j) {
    try {
        if (!j.is_object()) return std::nullopt;

        WorkflowGraph g;
        // Missing version => treat as v1 (forward-compatible with the very
        // first files we write before any migration exists).
        g.version = j.value("version", 1);
        g.name    = j.value("name", std::string{});

        if (auto it = j.find("nodes"); it != j.end() && it->is_array()) {
            g.nodes.reserve(it->size());
            for (const auto& jn : *it) g.nodes.push_back(nodeFromJson(jn));
        }
        if (auto it = j.find("edges"); it != j.end() && it->is_array()) {
            g.edges.reserve(it->size());
            for (const auto& je : *it) g.edges.push_back(edgeFromJson(je));
        }
        return g;
    } catch (const std::exception& e) {
        spdlog::warn("flow::fromJson: invalid workflow JSON: {}", e.what());
        return std::nullopt;
    }
}

bool saveToFile(const WorkflowGraph& g, const std::filesystem::path& path) {
    try {
        if (auto dir = path.parent_path(); !dir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                spdlog::error("flow::saveToFile mkdir {}: {}", dir.string(), ec.message());
                return false;
            }
        }

        std::filesystem::path tmp = path;
        tmp += ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f) {
                spdlog::error("flow::saveToFile open tmp {} failed", tmp.string());
                return false;
            }
            f << toJson(g).dump(2);
            if (!f) {
                spdlog::error("flow::saveToFile write tmp {} failed", tmp.string());
                return false;
            }
        }  // ofstream flushes + closes here

        std::error_code ec;
        if (!config::detail::atomicReplaceFile(tmp, path, ec)) {
            spdlog::error("flow::saveToFile rename {} -> {}: {}",
                          tmp.string(), path.string(), ec.message());
            std::error_code rm_ec;
            std::filesystem::remove(tmp, rm_ec);  // best-effort cleanup
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("flow::saveToFile exception: {}", e.what());
        return false;
    }
}

std::optional<WorkflowGraph> loadFromFile(const std::filesystem::path& path) {
    try {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            spdlog::warn("flow::loadFromFile cannot open {}", path.string());
            return std::nullopt;
        }
        json j;
        f >> j;
        return fromJson(j);
    } catch (const std::exception& e) {
        spdlog::warn("flow::loadFromFile parse {}: {}", path.string(), e.what());
        return std::nullopt;
    }
}

}  // namespace poebot::flow
