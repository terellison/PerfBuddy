#pragma once
//
// pb_runtime — runtime / frame-time hot spots.
//
// Ingests profiler output from the target's data dir:
//   *.folded         collapsed/folded stacks ("a;b;c <count>" per line)
//   *frametime*.csv  a frame-time series (milliseconds, one value per line)
// and reports the hottest call stacks, frame-time percentiles (p50/p95/p99),
// and frames that blow the budget.
//
#include "pb_core/analyzer.hpp"

namespace pb::runtime {

class RuntimeAnalyzer : public Analyzer {
public:
  std::string name() const override { return "pb_runtime"; }
  std::string description() const override {
    return "Runtime hot spots: hottest stacks, frame-time percentiles, spikes";
  }
  Category category() const override { return Category::Runtime; }
  bool supports(const Target& t) const override;
  ModuleReport analyze(const Target& t) const override;

  // Frame budget in milliseconds (default 60 FPS = 16.67 ms).
  double frame_budget_ms = 1000.0 / 60.0;
};

}  // namespace pb::runtime
