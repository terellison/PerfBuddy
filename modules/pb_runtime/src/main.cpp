#include "pb_core/report.hpp"
#include "pb_runtime/analyzer.hpp"

int main(int argc, char** argv) {
  pb::runtime::RuntimeAnalyzer analyzer;
  return pb::run_module_cli(analyzer, argc, argv);
}
