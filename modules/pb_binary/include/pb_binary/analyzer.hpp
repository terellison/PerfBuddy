#pragma once
//
// pb_binary — analyzes a built game executable's size and layout.
//
// Parses ELF (Linux), PE (Windows), and Mach-O (macOS) natively to report
// section/segment sizes, debug-info weight, whether the binary is stripped,
// and the heaviest symbols — the levers for shrinking executable size.
//
#include "pb_core/analyzer.hpp"

namespace pb::binary {

class BinaryAnalyzer : public Analyzer {
public:
  std::string name() const override { return "pb_binary"; }
  std::string description() const override {
    return "Executable size & layout: sections, debug info, symbols, bloat";
  }
  Category category() const override { return Category::BinarySize; }
  bool supports(const Target& t) const override;
  ModuleReport analyze(const Target& t) const override;
};

}  // namespace pb::binary
