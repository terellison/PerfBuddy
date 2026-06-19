#include "pb_core/report.hpp"
#include "pb_unreal/analyzer.hpp"

int main(int argc, char** argv) {
  pb::unreal::UnrealAnalyzer analyzer;
  return pb::run_module_cli(analyzer, argc, argv);
}
