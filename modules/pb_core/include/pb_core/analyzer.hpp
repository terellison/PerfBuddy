#pragma once
//
// The Analyzer interface. Every analysis module implements this and nothing
// else couples it to the rest of the system. The CLI and GUI both treat
// analyzers as an opaque list behind this interface.
//
#include <memory>
#include <string>
#include <vector>

#include "pb_core/types.hpp"

namespace pb {

class Analyzer {
public:
  virtual ~Analyzer() = default;

  // Stable module name, e.g. "pb_binary".
  virtual std::string name() const = 0;

  // Human-friendly one-line description (shown in the GUI / --list).
  virtual std::string description() const = 0;

  // Which concern this module covers (for grouping in reports).
  virtual Category category() const = 0;

  // Can this module do anything useful with the given target?
  virtual bool supports(const Target& target) const = 0;

  // Run the analysis. Implementations should not throw for ordinary
  // data problems; they should return a ModuleReport with `error` set.
  virtual ModuleReport analyze(const Target& target) const = 0;
};

using AnalyzerPtr = std::unique_ptr<Analyzer>;

}  // namespace pb
