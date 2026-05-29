#pragma once
#include <poebot/flow/graph.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>

// JSON (de)serialization for WorkflowGraph — the backbone of the import/export
// feature. Kept as explicit free functions (rather than nlohmann ADL hooks) so
// that:
//   - graph.hpp stays free of the JSON dependency, and
//   - fromJson/loadFromFile can VALIDATE untrusted imported files and report
//     failure via std::optional instead of throwing deep in nlohmann.
namespace poebot::flow {

// In-memory conversion. toJson never fails. fromJson returns nullopt when the
// JSON is missing required fields or has the wrong shape — callers treat that
// as "not a valid workflow file" and surface a user-facing error.
nlohmann::json               toJson(const WorkflowGraph& g);
std::optional<WorkflowGraph> fromJson(const nlohmann::json& j);

// File round-trip. saveToFile uses the project-standard atomic temp-file +
// rename (crash-safe, never leaves a half-written workflow on disk).
// loadFromFile returns nullopt on any read/parse/validation failure.
bool                         saveToFile(const WorkflowGraph& g,
                                        const std::filesystem::path& path);
std::optional<WorkflowGraph> loadFromFile(const std::filesystem::path& path);

}  // namespace poebot::flow
