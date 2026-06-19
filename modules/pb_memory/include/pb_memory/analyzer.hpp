#pragma once
//
// pb_memory — memory behaviour from an allocation log.
//
// Ingests an alloc/free log from the target's data dir. Two formats:
//   CSV : op,address,size[,timestamp][,tag]   where op is A/alloc or F/free
//   JSON: [ {"op":"A","addr":"0x..","size":N,"time":T,"tag":"..."} , ... ]
// Reports leak candidates (never freed), peak usage, the largest allocations,
// and allocation churn / fragmentation indicators.
//
#include "pb_core/analyzer.hpp"

namespace pb::memory {

class MemoryAnalyzer : public Analyzer {
public:
  std::string name() const override { return "pb_memory"; }
  std::string description() const override {
    return "Memory: leak candidates, peak usage, large allocations, churn";
  }
  Category category() const override { return Category::Memory; }
  bool supports(const Target& t) const override;
  ModuleReport analyze(const Target& t) const override;
};

}  // namespace pb::memory
