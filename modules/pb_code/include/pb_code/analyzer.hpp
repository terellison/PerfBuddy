#pragma once
//
// pb_code — C/C++ source heuristics for performance and quality.
//
// Walks the source tree and flags lightweight, high-signal anti-patterns:
// oversized translation units, heavy headers, `std::endl` in hot paths,
// raw new/delete, deep nesting, `using namespace std` in headers, and
// TODO/FIXME debt. Heuristic (no full parse) but fast and dependency-free.
//
#include "pb_core/analyzer.hpp"

namespace pb::code {

class CodeAnalyzer : public Analyzer {
public:
  std::string name() const override { return "pb_code"; }
  std::string description() const override {
    return "C/C++ source quality & perf anti-patterns (heuristic)";
  }
  Category category() const override { return Category::CodeQuality; }
  bool supports(const Target& t) const override;
  ModuleReport analyze(const Target& t) const override;
};

}  // namespace pb::code
