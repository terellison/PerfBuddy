#pragma once
//
// pb::core — the shared contract every PerfBuddy module speaks.
//
// Modules never depend on each other; they depend only on these types. Each
// module emits a ModuleReport made of Findings, and the orchestrator merges
// ModuleReports into a single Report. Everything serializes to JSON.
//
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "pb_core/json.hpp"

namespace pb {

enum class Severity { Info, Low, Medium, High, Critical };

enum class Category {
  BinarySize,
  Runtime,
  Memory,
  CodeQuality,
  Assets,
};

enum class Engine { Unknown, Native, Unreal };

std::string to_string(Severity s);
std::string to_string(Category c);
std::string to_string(Engine e);
Severity severity_from_string(const std::string& s);

// Where a finding lives: a file, an optional line, and/or a symbol/section name.
struct Location {
  std::string path;            // file path, or section/asset name
  std::optional<int> line;     // 1-based line, when known
  std::string symbol;          // symbol / section, when relevant

  json::Value to_json() const;
  std::string display() const;
};

// One actionable result.
struct Finding {
  std::string id;              // stable, e.g. "binary.large_section"
  std::string title;
  std::string description;
  Severity severity = Severity::Info;
  Category category = Category::CodeQuality;
  std::optional<Location> location;
  std::string impact;          // why it matters
  std::string remediation;     // what to do about it
  std::map<std::string, double> metrics;  // machine-readable evidence

  json::Value to_json() const;
};

// One module's output.
struct ModuleReport {
  std::string module;                    // e.g. "pb_binary"
  std::string version;
  std::string target_label;
  std::vector<Finding> findings;
  std::map<std::string, double> summary; // headline numbers
  long long duration_ms = 0;
  std::optional<std::string> error;      // set if the module failed

  json::Value to_json() const;
  // Count of findings at or above a severity.
  int count_at_least(Severity s) const;
};

// The full, merged result.
struct Report {
  std::string generated_at;              // ISO-8601 UTC
  std::vector<ModuleReport> modules;

  json::Value to_json() const;
  std::string dump(int indent = 2) const { return to_json().dump(indent); }
};

// The single input contract shared by every module.
struct Target {
  std::optional<std::string> executable;  // built game binary
  std::optional<std::string> source_dir;  // codebase root
  std::optional<std::string> data_dir;    // profiles / alloc logs / cooked assets
  Engine engine = Engine::Unknown;
  std::string label;                      // human label for reports

  // Best-effort engine detection from the provided paths.
  static Engine detect_engine(const Target& t);
};

}  // namespace pb
