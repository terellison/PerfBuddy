#pragma once
//
// pb_unreal — Unreal Engine-specific analysis.
//
// Detects an Unreal project, sizes cooked content (.pak/.uasset/.umap),
// flags oversized assets, scans Config/*.ini for costly settings, and applies
// Unreal C++ heuristics (heavy Tick, GetWorld()/Cast in Tick, GetAllActorsOfClass).
//
#include "pb_core/analyzer.hpp"

namespace pb::unreal {

class UnrealAnalyzer : public Analyzer {
public:
  std::string name() const override { return "pb_unreal"; }
  std::string description() const override {
    return "Unreal: cooked asset sizes, risky config, Tick/GC C++ heuristics";
  }
  Category category() const override { return Category::Assets; }
  bool supports(const Target& t) const override;
  ModuleReport analyze(const Target& t) const override;
};

}  // namespace pb::unreal
