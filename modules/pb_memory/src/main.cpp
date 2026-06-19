#include "pb_core/report.hpp"
#include "pb_memory/analyzer.hpp"

int main(int argc, char** argv) {
  pb::memory::MemoryAnalyzer analyzer;
  return pb::run_module_cli(analyzer, argc, argv);
}
