#include <fstream>
#include <iostream>

#include "pb_app/app.hpp"
#include "pb_core/report.hpp"
#include "pb_core/util.hpp"

using namespace pb;

namespace {

void print_help(const std::string& prog) {
  std::cout
      << "PerfBuddy — game performance & quality analysis suite\n\n"
      << "Usage:\n"
      << "  " << prog << " run [options]     analyze a target with all modules\n"
      << "  " << prog << " list              list available analysis modules\n"
      << "  " << prog << " --help\n\n"
      << "run options:\n"
      << "  --exe <path>      built game executable        (pb_binary)\n"
      << "  --src <dir>       source / codebase root        (pb_code, pb_unreal)\n"
      << "  --data <dir>      profiles / alloc logs / assets (pb_runtime, pb_memory, pb_unreal)\n"
      << "  --engine <name>   native | unreal | unknown      (default: auto-detect)\n"
      << "  --only <a,b,...>  restrict to named modules (e.g. pb_binary,pb_code)\n"
      << "  --format <fmt>    text | json                   (default: text)\n"
      << "  --output <file>   write report to a file instead of stdout\n"
      << "  --no-parallel     run modules sequentially\n\n"
      << "Example:\n"
      << "  " << prog << " run --exe ./game --src ./src --data ./profiling --format json\n";
}

int cmd_list() {
  std::cout << "Available modules:\n";
  for (const auto& m : app::list_modules())
    std::cout << "  " << m.name << "  [" << to_string(m.category) << "]\n      "
              << m.description << "\n";
  return 0;
}

Engine engine_from_string(const std::string& s) {
  if (s == "unreal") return Engine::Unreal;
  if (s == "native") return Engine::Native;
  return Engine::Unknown;
}

int cmd_run(const util::Args& args) {
  Target t;
  if (auto v = args.get("exe")) t.executable = *v;
  if (auto v = args.get("src")) t.source_dir = *v;
  if (auto v = args.get("data")) t.data_dir = *v;
  if (auto v = args.get("engine")) t.engine = engine_from_string(*v);
  t.label = t.executable.value_or(t.source_dir.value_or(t.data_dir.value_or("target")));

  if (!t.executable && !t.source_dir && !t.data_dir) {
    std::cerr << "error: provide at least one of --exe, --src, or --data\n"
                 "Run `perfbuddy run --help`-style flags or `perfbuddy --help`.\n";
    return 2;
  }

  std::vector<std::string> only;
  if (auto v = args.get("only")) only = util::split_csv(*v);
  bool parallel = !args.has("no-parallel");

  Report report = app::run_analysis(t, only, parallel);

  if (report.modules.empty()) {
    std::cerr << "No module could analyze the given target. "
                 "Check that the paths exist and match what each module expects "
                 "(`perfbuddy list`).\n";
    return 1;
  }

  std::string format = args.get_or("format", "text");
  std::string rendered =
      (format == "json") ? report.dump(2) : render_text(report);

  if (auto out = args.get("output")) {
    std::ofstream f(*out);
    if (!f) { std::cerr << "could not open output: " << *out << "\n"; return 1; }
    f << rendered << "\n";
    std::cerr << "wrote " << *out << "\n";
  } else {
    std::cout << rendered << "\n";
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  util::Args args(argc, argv);
  const auto& pos = args.positionals();
  std::string prog = "perfbuddy";

  if (args.has("help") || pos.empty()) {
    print_help(prog);
    return pos.empty() && !args.has("help") ? 1 : 0;
  }
  const std::string& cmd = pos.front();
  if (cmd == "list") return cmd_list();
  if (cmd == "run") return cmd_run(args);

  std::cerr << "unknown command: " << cmd << "\n";
  print_help(prog);
  return 2;
}
