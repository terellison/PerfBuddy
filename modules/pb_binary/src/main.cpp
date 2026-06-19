#include "pb_binary/analyzer.hpp"
#include "pb_core/report.hpp"

int main(int argc, char** argv) {
  pb::binary::BinaryAnalyzer analyzer;
  return pb::run_module_cli(analyzer, argc, argv);
}
