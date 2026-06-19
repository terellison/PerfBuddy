#include "pb_memory/analyzer.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <map>
#include <sstream>
#include <unordered_map>

#include "pb_core/json.hpp"
#include "pb_core/util.hpp"

namespace pb::memory {

namespace {

struct Event {
  bool alloc = true;
  std::string addr;
  double size = 0;
  double time = 0;
  std::string tag;
};

std::vector<std::string> split_line(const std::string& line, char delim) {
  std::vector<std::string> out;
  std::string cur;
  std::istringstream ss(line);
  while (std::getline(ss, cur, delim)) {
    size_t a = cur.find_first_not_of(" \t\r");
    size_t b = cur.find_last_not_of(" \t\r");
    out.push_back(a == std::string::npos ? "" : cur.substr(a, b - a + 1));
  }
  return out;
}

bool is_alloc_op(const std::string& s) {
  if (s.empty()) return true;
  char c = std::tolower(static_cast<unsigned char>(s[0]));
  return c == 'a' || c == 'm';  // alloc / malloc
}

bool parse_csv(const std::string& contents, std::vector<Event>& events) {
  std::istringstream ss(contents);
  std::string line;
  bool first = true;
  while (std::getline(ss, line)) {
    if (line.empty()) continue;
    auto cols = split_line(line, ',');
    if (cols.size() < 3) continue;
    if (first) {
      first = false;
      // Skip a header row (size column not numeric).
      bool numeric = !cols[2].empty() &&
                     (std::isdigit(static_cast<unsigned char>(cols[2][0])) ||
                      cols[2][0] == '0');
      if (!numeric) continue;
    }
    Event e;
    e.alloc = is_alloc_op(cols[0]);
    e.addr = cols[1];
    try { e.size = std::stod(cols[2]); } catch (...) { continue; }
    if (cols.size() > 3 && !cols[3].empty()) { try { e.time = std::stod(cols[3]); } catch (...) {} }
    if (cols.size() > 4) e.tag = cols[4];
    events.push_back(std::move(e));
  }
  return !events.empty();
}

bool parse_json(const std::string& contents, std::vector<Event>& events) {
  try {
    auto v = json::Value::parse(contents);
    if (!v.is_array()) return false;
    for (const auto& item : v.as_array()) {
      if (!item.is_object()) continue;
      Event e;
      e.alloc = is_alloc_op(item["op"].as_string(""));
      e.addr = item["addr"].as_string("");
      e.size = item["size"].as_number(0);
      e.time = item["time"].as_number(0);
      e.tag = item["tag"].as_string("");
      events.push_back(std::move(e));
    }
  } catch (...) {
    return false;
  }
  return !events.empty();
}

}  // namespace

bool MemoryAnalyzer::supports(const Target& t) const {
  if (!t.data_dir || !util::is_directory(*t.data_dir)) return false;
  for (const auto& f : util::list_files(*t.data_dir)) {
    std::string name = util::filename_of(f);
    if (name.find("alloc") != std::string::npos ||
        name.find("mem") != std::string::npos ||
        name.find("heap") != std::string::npos)
      return true;
  }
  return false;
}

ModuleReport MemoryAnalyzer::analyze(const Target& t) const {
  auto start = std::chrono::steady_clock::now();
  ModuleReport report;
  report.module = name();
  report.version = "0.1.0";
  report.target_label = t.label;

  std::vector<Event> events;
  for (const auto& path : util::list_files(*t.data_dir)) {
    std::string name = util::filename_of(path);
    if (name.find("alloc") == std::string::npos &&
        name.find("mem") == std::string::npos &&
        name.find("heap") == std::string::npos)
      continue;
    auto contents = util::read_file(path);
    if (!contents) continue;
    if (util::extension_of(path) == ".json")
      parse_json(*contents, events);
    else
      parse_csv(*contents, events);
  }

  if (events.empty()) {
    report.error = "no allocation events parsed from data dir";
    return report;
  }

  // Replay the log.
  std::unordered_map<std::string, std::pair<double, std::string>> live;  // addr -> (size,tag)
  double current = 0, peak = 0;
  long long n_alloc = 0, n_free = 0, double_free = 0;
  double total_allocated = 0;
  double max_single = 0;
  std::map<std::string, double> leak_by_tag;
  std::vector<std::pair<double, std::string>> alloc_sizes;  // (size,tag) for top-N

  for (const auto& e : events) {
    if (e.alloc) {
      ++n_alloc;
      total_allocated += e.size;
      current += e.size;
      peak = std::max(peak, current);
      max_single = std::max(max_single, e.size);
      live[e.addr] = {e.size, e.tag};
      alloc_sizes.emplace_back(e.size, e.tag);
    } else {
      auto it = live.find(e.addr);
      if (it == live.end()) {
        ++double_free;
      } else {
        ++n_free;
        current -= it->second.first;
        if (current < 0) current = 0;
        live.erase(it);
      }
    }
  }

  double leaked = 0;
  for (const auto& kv : live) {
    leaked += kv.second.first;
    leak_by_tag[kv.second.second.empty() ? "<untagged>" : kv.second.second] +=
        kv.second.first;
  }

  report.summary["events"] = static_cast<double>(events.size());
  report.summary["allocations"] = static_cast<double>(n_alloc);
  report.summary["frees"] = static_cast<double>(n_free);
  report.summary["peak_bytes"] = peak;
  report.summary["leaked_bytes"] = leaked;
  report.summary["leak_blocks"] = static_cast<double>(live.size());

  auto add = [&](Finding f) { report.findings.push_back(std::move(f)); };

  // Overview.
  {
    Finding f;
    f.id = "memory.overview";
    f.category = Category::Memory;
    f.severity = Severity::Info;
    f.title = "Peak heap " + util::human_bytes(peak) + ", " +
              std::to_string(n_alloc) + " allocs / " + std::to_string(n_free) + " frees";
    f.impact = "Baseline working set and allocator traffic.";
    f.remediation = "Compare peak against the platform budget; reduce churn below.";
    f.metrics["peak_bytes"] = peak;
    f.metrics["allocations"] = static_cast<double>(n_alloc);
    add(std::move(f));
  }

  // Leaks.
  if (leaked > 0) {
    Finding f;
    f.id = "memory.leak";
    f.category = Category::Memory;
    f.severity = leaked > 16 * 1024 * 1024 ? Severity::Critical
                 : leaked > 1024 * 1024 ? Severity::High
                                        : Severity::Medium;
    f.title = util::human_bytes(leaked) + " never freed across " +
              std::to_string(live.size()) + " blocks (leak candidates)";
    std::string by;
    std::vector<std::pair<std::string, double>> tags(leak_by_tag.begin(), leak_by_tag.end());
    std::sort(tags.begin(), tags.end(), [](auto& a, auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < tags.size() && i < 3; ++i)
      by += (i ? ", " : "") + tags[i].first + "=" + util::human_bytes(tags[i].second);
    f.description = "Top leak sources: " + by;
    f.impact = "Grows working set over a session; can trigger OOM crashes.";
    f.remediation = "Pair each allocation with a free (RAII / smart pointers); "
                    "audit the top tags above with a leak sanitizer.";
    f.metrics["leaked_bytes"] = leaked;
    f.metrics["leak_blocks"] = static_cast<double>(live.size());
    add(std::move(f));
  }

  // Double / invalid frees.
  if (double_free > 0) {
    Finding f;
    f.id = "memory.invalid_free";
    f.category = Category::Memory;
    f.severity = Severity::High;
    f.title = std::to_string(double_free) + " frees of unknown/already-freed addresses";
    f.impact = "Double-free / invalid-free is undefined behaviour and a crash/security risk.";
    f.remediation = "Null pointers after free; use sanitizers (ASan) to find the source.";
    f.metrics["invalid_frees"] = static_cast<double>(double_free);
    add(std::move(f));
  }

  // Largest single allocation.
  if (max_single > 0) {
    std::sort(alloc_sizes.begin(), alloc_sizes.end(),
              [](auto& a, auto& b) { return a.first > b.first; });
    Finding f;
    f.id = "memory.large_alloc";
    f.category = Category::Memory;
    f.severity = max_single > 64 * 1024 * 1024 ? Severity::Medium : Severity::Low;
    f.title = "Largest allocation " + util::human_bytes(max_single) +
              (alloc_sizes[0].second.empty() ? "" : " (" + alloc_sizes[0].second + ")");
    f.impact = "Large contiguous allocations cause fragmentation and stalls.";
    f.remediation = "Pool or stream large buffers; pre-reserve instead of growing.";
    f.metrics["bytes"] = max_single;
    add(std::move(f));
  }

  // Churn / fragmentation indicator.
  double avg = n_alloc ? total_allocated / n_alloc : 0;
  if (n_alloc > 0 && avg < 128 && n_alloc > 1000) {
    Finding f;
    f.id = "memory.churn";
    f.category = Category::Memory;
    f.severity = Severity::Medium;
    f.title = "High small-allocation churn: " + std::to_string(n_alloc) +
              " allocs averaging " + util::human_bytes(avg);
    f.impact = "Frequent tiny allocations fragment the heap and cost allocator time.";
    f.remediation = "Use object pools / arena allocators and reserve() containers.";
    f.metrics["avg_alloc_bytes"] = avg;
    f.metrics["allocations"] = static_cast<double>(n_alloc);
    add(std::move(f));
  }

  report.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start).count();
  return report;
}

}  // namespace pb::memory
