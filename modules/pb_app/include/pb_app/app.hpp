#pragma once
//
// pb_app — the composition layer.
//
// This is the ONLY place that knows about every analysis module at once. It
// builds the registry of analyzers and runs them against a Target, merging the
// results into one Report. Both the CLI and the GUI call into here, so they
// share identical behaviour and own no analysis logic themselves.
//
#include <string>
#include <vector>

#include "pb_core/analyzer.hpp"
#include "pb_core/types.hpp"

namespace pb::app {

// One entry per registered module (used to list/select modules in UIs).
struct ModuleInfo {
  std::string name;
  std::string description;
  Category category;
};

// Construct the default set of analyzers (one per concern).
std::vector<AnalyzerPtr> make_default_analyzers();

// Metadata for all registered modules, without running anything.
std::vector<ModuleInfo> list_modules();

// Run analysis against a target.
//   `only`     : if non-empty, restrict to these module names (e.g. "pb_code").
//   `parallel` : run supported modules concurrently.
// Modules whose supports() is false are skipped silently.
Report run_analysis(const Target& target,
                    const std::vector<std::string>& only = {},
                    bool parallel = true);

}  // namespace pb::app
