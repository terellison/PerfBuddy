#include "pb_code/analyzer.hpp"
#include "pb_core/report.hpp"

int main(int argc, char** argv) {
  pb::code::CodeAnalyzer analyzer;
  return pb::run_module_cli(analyzer, argc, argv);
}
