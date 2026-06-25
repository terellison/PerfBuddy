#include "pb_core/report.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include "pb_core/util.hpp"

namespace pb {

namespace {

const char* severity_tag(Severity s) {
  switch (s) {
    case Severity::Info: return "[INFO]";
    case Severity::Low: return "[LOW ]";
    case Severity::Medium: return "[MED ]";
    case Severity::High: return "[HIGH]";
    case Severity::Critical: return "[CRIT]";
  }
  return "[INFO]";
}

void render_module_into(std::ostringstream& out, const ModuleReport& r) {
  out << "== " << r.module << " (" << r.findings.size() << " findings, "
      << r.duration_ms << " ms) ==\n";
  if (r.error) {
    out << "  ERROR: " << *r.error << "\n\n";
    return;
  }
  if (!r.summary.empty()) {
    out << "  summary: ";
    bool first = true;
    for (const auto& kv : r.summary) {
      if (!first) out << ", ";
      out << kv.first << "=" << kv.second;
      first = false;
    }
    out << "\n";
  }
  // Highest severity first.
  std::vector<const Finding*> sorted;
  for (const auto& f : r.findings) sorted.push_back(&f);
  std::stable_sort(sorted.begin(), sorted.end(), [](const Finding* a, const Finding* b) {
    return static_cast<int>(a->severity) > static_cast<int>(b->severity);
  });
  for (const Finding* f : sorted) {
    out << "  " << severity_tag(f->severity) << " " << f->title;
    if (f->location) out << "  @ " << f->location->display();
    out << "\n";
    if (!f->description.empty()) out << "        " << f->description << "\n";
    if (!f->remediation.empty()) out << "        fix: " << f->remediation << "\n";
  }
  if (r.findings.empty()) out << "  (no findings)\n";
  out << "\n";
}

}  // namespace

std::string render_text(const ModuleReport& report) {
  std::ostringstream out;
  render_module_into(out, report);
  return out.str();
}

std::string render_text(const Report& report) {
  std::ostringstream out;
  out << "PerfBuddy report — " << report.generated_at << "\n";
  int total = 0, high = 0;
  for (const auto& m : report.modules) {
    total += static_cast<int>(m.findings.size());
    high += m.count_at_least(Severity::High);
  }
  out << report.modules.size() << " module(s), " << total << " finding(s), "
      << high << " high/critical\n\n";
  for (const auto& m : report.modules) render_module_into(out, m);
  return out.str();
}

namespace {

void print_help(const Analyzer& a, const std::string& prog) {
  std::cout
      << a.name() << " — " << a.description() << "\n\n"
      << "Usage: " << prog << " [options]\n\n"
      << "Options:\n"
      << "  --exe <path>      path to the built game executable\n"
      << "  --src <dir>       path to the source/codebase root\n"
      << "  --data <dir>      path to profiling / allocation / asset inputs\n"
      << "  --engine <name>   native | unreal | unknown (default: auto-detect)\n"
      << "  --rules <file>    .editorconfig-style rule severity overrides\n"
      << "  --format <fmt>    text | json (default: text)\n"
      << "  --output <file>   write to a file instead of stdout\n"
      << "  --help            show this help\n";
}

Engine engine_from_string(const std::string& s) {
  if (s == "unreal") return Engine::Unreal;
  if (s == "native") return Engine::Native;
  return Engine::Unknown;
}

}  // namespace

int run_module_cli(Analyzer& analyzer, int argc, char** argv) {
  util::Args args(argc, argv);
  if (args.has("help")) {
    print_help(analyzer, args.program());
    return 0;
  }

  Target t;
  if (auto v = args.get("exe")) t.executable = *v;
  if (auto v = args.get("src")) t.source_dir = *v;
  if (auto v = args.get("data")) t.data_dir = *v;
  if (auto v = args.get("engine")) t.engine = engine_from_string(*v);
  if (auto v = args.get("rules")) t.rules_file = *v;
  t.engine = Target::detect_engine(t);
  t.label = t.executable.value_or(t.source_dir.value_or(t.data_dir.value_or("target")));

  if (!analyzer.supports(t)) {
    std::cerr << analyzer.name() << ": this target has no inputs this module can use.\n"
              << "Try --help for the inputs it expects.\n";
    return 2;
  }

  ModuleReport report = analyzer.analyze(t);

  std::string format = args.get_or("format", "text");
  std::string rendered =
      (format == "json") ? report.to_json().dump(2) : render_text(report);

  if (auto out = args.get("output")) {
    std::ofstream f(*out);
    if (!f) {
      std::cerr << "could not open output file: " << *out << "\n";
      return 1;
    }
    f << rendered << "\n";
    std::cerr << "wrote " << *out << "\n";
  } else {
    std::cout << rendered << "\n";
  }
  return report.error ? 1 : 0;
}

}  // namespace pb
