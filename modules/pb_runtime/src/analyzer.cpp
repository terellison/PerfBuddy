#include "pb_runtime/analyzer.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <map>
#include <sstream>

#include "pb_core/util.hpp"

namespace pb::runtime {

namespace {

// Pull the first floating-point number out of a line; nullopt if none.
std::optional<double> first_number(const std::string& line) {
  size_t i = 0;
  while (i < line.size()) {
    char c = line[i];
    if (std::isdigit(static_cast<unsigned char>(c)) ||
        ((c == '-' || c == '+' || c == '.') && i + 1 < line.size() &&
         std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
      size_t start = i;
      while (i < line.size() &&
             (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.' ||
              line[i] == 'e' || line[i] == 'E' || line[i] == '+' || line[i] == '-'))
        ++i;
      try {
        return std::stod(line.substr(start, i - start));
      } catch (...) {
        return std::nullopt;
      }
    }
    ++i;
  }
  return std::nullopt;
}

double percentile(std::vector<double>& sorted, double pct) {
  if (sorted.empty()) return 0.0;
  double idx = pct / 100.0 * (sorted.size() - 1);
  size_t lo = static_cast<size_t>(std::floor(idx));
  size_t hi = static_cast<size_t>(std::ceil(idx));
  double frac = idx - lo;
  return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

struct StackSample {
  std::string stack;
  long long count = 0;
};

}  // namespace

bool RuntimeAnalyzer::supports(const Target& t) const {
  if (!t.data_dir || !util::is_directory(*t.data_dir)) return false;
  for (const auto& f : util::list_files(*t.data_dir)) {
    std::string name = util::filename_of(f);
    std::string ext = util::extension_of(f);
    if (ext == ".folded") return true;
    if (name.find("frametime") != std::string::npos ||
        name.find("frame_time") != std::string::npos ||
        name.find("frames") != std::string::npos)
      return true;
  }
  return false;
}

ModuleReport RuntimeAnalyzer::analyze(const Target& t) const {
  auto start = std::chrono::steady_clock::now();
  ModuleReport report;
  report.module = name();
  report.version = "0.1.0";
  report.target_label = t.label;

  std::vector<StackSample> stacks;
  std::map<std::string, long long> leaf_self;  // self-time by leaf function
  long long total_samples = 0;
  std::vector<double> frametimes;

  for (const auto& path : util::list_files(*t.data_dir)) {
    std::string name = util::filename_of(path);
    std::string ext = util::extension_of(path);
    auto contents = util::read_file(path);
    if (!contents) continue;

    if (ext == ".folded") {
      std::istringstream ss(*contents);
      std::string line;
      while (std::getline(ss, line)) {
        if (line.empty()) continue;
        size_t sp = line.find_last_of(" \t");
        if (sp == std::string::npos) continue;
        std::string stack = line.substr(0, sp);
        long long count = 0;
        try { count = std::stoll(line.substr(sp + 1)); } catch (...) { continue; }
        stacks.push_back({stack, count});
        total_samples += count;
        size_t leafpos = stack.find_last_of(';');
        std::string leaf = leafpos == std::string::npos ? stack
                                                        : stack.substr(leafpos + 1);
        leaf_self[leaf] += count;
      }
    } else if (name.find("frametime") != std::string::npos ||
               name.find("frame_time") != std::string::npos ||
               name.find("frames") != std::string::npos) {
      std::istringstream ss(*contents);
      std::string line;
      bool first_line = true;
      while (std::getline(ss, line)) {
        if (first_line) {
          first_line = false;
          // Skip an obvious header row (no leading digit).
          std::string trimmed = line;
          trimmed.erase(0, trimmed.find_first_not_of(" \t"));
          if (!trimmed.empty() && !std::isdigit(static_cast<unsigned char>(trimmed[0])) &&
              trimmed[0] != '.' && trimmed[0] != '-')
            continue;
        }
        if (auto v = first_number(line)) frametimes.push_back(*v);
      }
    }
  }

  if (stacks.empty() && frametimes.empty()) {
    report.error = "no profiler (.folded) or frame-time data found in data dir";
    return report;
  }

  auto add = [&](Finding f) { report.findings.push_back(std::move(f)); };

  // ---- Frame-time analysis ----
  if (!frametimes.empty()) {
    std::vector<double> sorted = frametimes;
    std::sort(sorted.begin(), sorted.end());
    double p50 = percentile(sorted, 50);
    double p95 = percentile(sorted, 95);
    double p99 = percentile(sorted, 99);
    double maxv = sorted.back();
    double mean = 0;
    for (double v : frametimes) mean += v;
    mean /= frametimes.size();
    long long over = 0;
    for (double v : frametimes) if (v > frame_budget_ms) ++over;
    double over_pct = 100.0 * over / frametimes.size();

    report.summary["frames"] = static_cast<double>(frametimes.size());
    report.summary["p50_ms"] = p50;
    report.summary["p95_ms"] = p95;
    report.summary["p99_ms"] = p99;
    report.summary["over_budget_pct"] = over_pct;

    {
      Finding f;
      f.id = "runtime.frame_percentiles";
      f.category = Category::Runtime;
      char buf[160];
      std::snprintf(buf, sizeof(buf),
                    "Frame time p50=%.2f p95=%.2f p99=%.2f max=%.2f ms (budget %.2f)",
                    p50, p95, p99, maxv, frame_budget_ms);
      f.title = buf;
      f.severity = p95 > frame_budget_ms ? Severity::High
                   : p99 > frame_budget_ms ? Severity::Medium
                                           : Severity::Info;
      f.description = std::to_string(frametimes.size()) + " frames analyzed; mean " +
                      std::to_string(mean) + " ms.";
      f.impact = p95 > frame_budget_ms
                     ? "Most frames miss the budget — visible stutter / low FPS."
                     : "Tail frames occasionally miss the budget.";
      f.remediation = "Profile the hottest stacks below; move work off the critical "
                      "path, batch draw calls, and smooth per-frame allocations.";
      f.metrics["p50_ms"] = p50; f.metrics["p95_ms"] = p95;
      f.metrics["p99_ms"] = p99; f.metrics["max_ms"] = maxv;
      add(std::move(f));
    }
    if (over > 0) {
      Finding f;
      f.id = "runtime.budget_spikes";
      f.category = Category::Runtime;
      f.severity = over_pct > 10 ? Severity::High
                   : over_pct > 1 ? Severity::Medium
                                  : Severity::Low;
      char buf[160];
      std::snprintf(buf, sizeof(buf), "%lld frames (%.1f%%) exceed the %.2f ms budget",
                    over, over_pct, frame_budget_ms);
      f.title = buf;
      f.impact = "Each spike is a dropped frame the player can feel.";
      f.remediation = "Investigate GC/allocation hitches, asset streaming, and "
                      "synchronous loads on the main thread.";
      f.metrics["spikes"] = static_cast<double>(over);
      f.metrics["over_budget_pct"] = over_pct;
      add(std::move(f));
    }
  }

  // ---- Hot-stack analysis ----
  if (!stacks.empty()) {
    report.summary["stack_samples"] = static_cast<double>(total_samples);

    std::sort(stacks.begin(), stacks.end(),
              [](const StackSample& a, const StackSample& b) { return a.count > b.count; });
    int n = 0;
    for (const auto& s : stacks) {
      if (n++ >= 5) break;
      double share = total_samples ? static_cast<double>(s.count) / total_samples : 0;
      Finding f;
      f.id = "runtime.hot_stack";
      f.category = Category::Runtime;
      f.severity = share > 0.25 ? Severity::High
                   : share > 0.10 ? Severity::Medium
                                  : Severity::Low;
      std::string label = s.stack.size() > 90 ? "..." + s.stack.substr(s.stack.size() - 87)
                                              : s.stack;
      f.title = "Hot stack (" + std::to_string(static_cast<int>(share * 100)) +
                "% of samples): " + label;
      f.impact = "Concentrated CPU time — the biggest optimization lever.";
      f.remediation = "Optimize the leaf function, reduce call frequency, or "
                      "parallelize / cache its results.";
      f.metrics["samples"] = static_cast<double>(s.count);
      f.metrics["share"] = share;
      add(std::move(f));
    }

    // Top self-time leaves.
    std::vector<std::pair<std::string, long long>> leaves(leaf_self.begin(),
                                                          leaf_self.end());
    std::sort(leaves.begin(), leaves.end(),
              [](auto& a, auto& b) { return a.second > b.second; });
    int m = 0;
    for (const auto& l : leaves) {
      if (m++ >= 3) break;
      double share = total_samples ? static_cast<double>(l.second) / total_samples : 0;
      if (share < 0.05) break;
      Finding f;
      f.id = "runtime.hot_leaf";
      f.category = Category::Runtime;
      f.severity = share > 0.20 ? Severity::Medium : Severity::Low;
      f.title = "Hot function " + l.first + " (" +
                std::to_string(static_cast<int>(share * 100)) + "% self time)";
      f.impact = "Function where the CPU actually spends its time.";
      f.remediation = "Micro-optimize, vectorize, or call it less often.";
      f.metrics["samples"] = static_cast<double>(l.second);
      f.metrics["share"] = share;
      add(std::move(f));
    }
  }

  report.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start).count();
  return report;
}

}  // namespace pb::runtime
