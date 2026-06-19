#pragma once
//
// Rendering + a reusable entry point for the standalone module CLIs.
// Every `pb-<module>` binary is just: build a Target from flags, run one
// Analyzer, and print. That logic lives here so each module's main() is ~3 lines.
//
#include <string>

#include "pb_core/analyzer.hpp"
#include "pb_core/types.hpp"

namespace pb {

// Render a single module report as readable plain text.
std::string render_text(const ModuleReport& report);

// Render a full merged report as readable plain text.
std::string render_text(const Report& report);

// Shared main() for a standalone module binary. Parses common flags
// (--exe --src --data --engine --format json|text --output FILE --help),
// runs the analyzer, prints, and returns a process exit code.
int run_module_cli(Analyzer& analyzer, int argc, char** argv);

}  // namespace pb
