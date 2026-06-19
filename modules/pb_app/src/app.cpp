#include "pb_app/app.hpp"

#include <algorithm>
#include <future>

#include "pb_binary/analyzer.hpp"
#include "pb_code/analyzer.hpp"
#include "pb_core/util.hpp"
#include "pb_memory/analyzer.hpp"
#include "pb_runtime/analyzer.hpp"
#include "pb_unreal/analyzer.hpp"

namespace pb::app {

std::vector<AnalyzerPtr> make_default_analyzers() {
  std::vector<AnalyzerPtr> v;
  v.push_back(std::make_unique<binary::BinaryAnalyzer>());
  v.push_back(std::make_unique<runtime::RuntimeAnalyzer>());
  v.push_back(std::make_unique<memory::MemoryAnalyzer>());
  v.push_back(std::make_unique<code::CodeAnalyzer>());
  v.push_back(std::make_unique<unreal::UnrealAnalyzer>());
  return v;
}

std::vector<ModuleInfo> list_modules() {
  std::vector<ModuleInfo> out;
  for (const auto& a : make_default_analyzers())
    out.push_back({a->name(), a->description(), a->category()});
  return out;
}

namespace {
bool selected(const std::vector<std::string>& only, const std::string& name) {
  if (only.empty()) return true;
  return std::find(only.begin(), only.end(), name) != only.end();
}
}  // namespace

Report run_analysis(const Target& target_in, const std::vector<std::string>& only,
                    bool parallel) {
  Target target = target_in;
  target.engine = Target::detect_engine(target);

  Report report;
  report.generated_at = util::iso8601_now();

  auto analyzers = make_default_analyzers();

  // Decide which analyzers actually run.
  std::vector<Analyzer*> to_run;
  for (auto& a : analyzers)
    if (selected(only, a->name()) && a->supports(target)) to_run.push_back(a.get());

  if (parallel && to_run.size() > 1) {
    std::vector<std::future<ModuleReport>> futures;
    for (Analyzer* a : to_run)
      futures.push_back(std::async(std::launch::async, [a, &target] {
        try {
          return a->analyze(target);
        } catch (const std::exception& e) {
          ModuleReport r;
          r.module = a->name();
          r.error = std::string("analyzer threw: ") + e.what();
          return r;
        }
      }));
    for (auto& f : futures) report.modules.push_back(f.get());
  } else {
    for (Analyzer* a : to_run) {
      try {
        report.modules.push_back(a->analyze(target));
      } catch (const std::exception& e) {
        ModuleReport r;
        r.module = a->name();
        r.error = std::string("analyzer threw: ") + e.what();
        report.modules.push_back(r);
      }
    }
  }

  // Keep output order stable regardless of completion order.
  std::sort(report.modules.begin(), report.modules.end(),
            [](const ModuleReport& a, const ModuleReport& b) {
              return a.module < b.module;
            });
  return report;
}

}  // namespace pb::app
