#include "pb_code/analyzer.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

#include "pb_core/util.hpp"

namespace pb::code {

namespace {

bool is_header(const std::string& ext) {
  return ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".hxx";
}

// Strip // and /* */ comments so heuristics don't fire inside comments.
// Returns the cleaned source (line structure preserved).
std::string strip_comments(const std::string& src) {
  std::string out;
  out.reserve(src.size());
  bool in_block = false, in_line = false, in_str = false, in_char = false;
  for (size_t i = 0; i < src.size(); ++i) {
    char c = src[i];
    char n = i + 1 < src.size() ? src[i + 1] : '\0';
    if (in_line) {
      if (c == '\n') { in_line = false; out += c; }
      continue;
    }
    if (in_block) {
      if (c == '*' && n == '/') { in_block = false; ++i; }
      else if (c == '\n') out += c;
      continue;
    }
    if (in_str) {
      out += c;
      if (c == '\\') { if (i + 1 < src.size()) { out += src[++i]; } }
      else if (c == '"') in_str = false;
      continue;
    }
    if (in_char) {
      out += c;
      if (c == '\\') { if (i + 1 < src.size()) { out += src[++i]; } }
      else if (c == '\'') in_char = false;
      continue;
    }
    if (c == '/' && n == '/') { in_line = true; continue; }
    if (c == '/' && n == '*') { in_block = true; ++i; continue; }
    if (c == '"') { in_str = true; out += c; continue; }
    if (c == '\'') { in_char = true; out += c; continue; }
    out += c;
  }
  return out;
}

struct Counts {
  long long endl = 0;
  long long raw_new = 0;
  long long raw_delete = 0;
  long long todos = 0;
  long long includes = 0;
};

size_t count_occurrences(const std::string& hay, const std::string& needle) {
  size_t n = 0, pos = 0;
  while ((pos = hay.find(needle, pos)) != std::string::npos) { ++n; pos += needle.size(); }
  return n;
}

int max_brace_depth(const std::string& src) {
  int depth = 0, maxd = 0;
  for (char c : src) {
    if (c == '{') { if (++depth > maxd) maxd = depth; }
    else if (c == '}') { if (depth > 0) --depth; }
  }
  return maxd;
}

}  // namespace

bool CodeAnalyzer::supports(const Target& t) const {
  return t.source_dir && util::is_directory(*t.source_dir);
}

ModuleReport CodeAnalyzer::analyze(const Target& t) const {
  auto begin = std::chrono::steady_clock::now();
  ModuleReport report;
  report.module = name();
  report.version = "0.1.0";
  report.target_label = t.label;

  const std::vector<std::string> exts = {".c",  ".cc",  ".cpp", ".cxx",
                                         ".h",  ".hh",  ".hpp", ".hxx"};
  auto files = util::list_files(*t.source_dir, exts);
  if (files.empty()) {
    report.error = "no C/C++ source files found under " + *t.source_dir;
    return report;
  }

  Counts total;
  long long total_lines = 0;
  std::vector<std::pair<std::string, long long>> big_files;       // path, lines
  std::vector<std::pair<std::string, long long>> heavy_headers;   // path, includes
  std::vector<std::pair<std::string, int>> deep_files;            // path, depth
  std::vector<std::string> using_std_headers;                     // header paths
  Location first_endl, first_new;
  bool have_first_endl = false, have_first_new = false;

  for (const auto& path : files) {
    auto raw = util::read_file(path);
    if (!raw) continue;
    std::string ext = util::extension_of(path);
    std::string code = strip_comments(*raw);

    long long lines = 1 + std::count(raw->begin(), raw->end(), '\n');
    total_lines += lines;

    Counts c;
    c.endl = count_occurrences(code, "std::endl");
    c.raw_new = count_occurrences(code, " new ");
    c.raw_delete = count_occurrences(code, "delete ");
    c.includes = count_occurrences(code, "#include");
    c.todos = count_occurrences(*raw, "TODO") + count_occurrences(*raw, "FIXME") +
              count_occurrences(*raw, "XXX") + count_occurrences(*raw, "HACK");

    total.endl += c.endl;
    total.raw_new += c.raw_new;
    total.raw_delete += c.raw_delete;
    total.includes += c.includes;
    total.todos += c.todos;

    if (!have_first_endl && c.endl > 0) {
      first_endl.path = path; have_first_endl = true;
    }
    if (!have_first_new && c.raw_new > 0) {
      first_new.path = path; have_first_new = true;
    }

    // Oversized translation unit (implementation files only).
    if (!is_header(ext) && lines > 1500) big_files.emplace_back(path, lines);

    // Heavy headers (compile-time cost).
    if (is_header(ext) && (c.includes > 25 || lines > 800))
      heavy_headers.emplace_back(path, c.includes);

    // using namespace std in a header — leaks into every TU.
    if (is_header(ext) && code.find("using namespace std") != std::string::npos)
      using_std_headers.push_back(path);

    int depth = max_brace_depth(code);
    if (depth >= 7) deep_files.emplace_back(path, depth);
  }

  report.summary["files"] = static_cast<double>(files.size());
  report.summary["lines"] = static_cast<double>(total_lines);
  report.summary["todos"] = static_cast<double>(total.todos);

  auto add = [&](Finding f) { report.findings.push_back(std::move(f)); };

  // Overview.
  {
    Finding f;
    f.id = "code.overview";
    f.category = Category::CodeQuality;
    f.severity = Severity::Info;
    f.title = std::to_string(files.size()) + " source files, " +
              std::to_string(total_lines) + " lines scanned";
    f.impact = "Baseline for the heuristics below.";
    f.remediation = "Review the flagged hot spots.";
    add(std::move(f));
  }

  std::sort(big_files.begin(), big_files.end(),
            [](auto& a, auto& b) { return a.second > b.second; });
  for (size_t i = 0; i < big_files.size() && i < 5; ++i) {
    Finding f;
    f.id = "code.oversized_tu";
    f.category = Category::CodeQuality;
    f.severity = big_files[i].second > 4000 ? Severity::Medium : Severity::Low;
    f.title = "Large translation unit: " + util::filename_of(big_files[i].first) +
              " (" + std::to_string(big_files[i].second) + " lines)";
    Location loc; loc.path = big_files[i].first; f.location = loc;
    f.impact = "Slow to compile and recompile; hard to maintain and review.";
    f.remediation = "Split into focused units; move definitions out of one mega-file.";
    f.metrics["lines"] = static_cast<double>(big_files[i].second);
    add(std::move(f));
  }

  std::sort(heavy_headers.begin(), heavy_headers.end(),
            [](auto& a, auto& b) { return a.second > b.second; });
  for (size_t i = 0; i < heavy_headers.size() && i < 5; ++i) {
    Finding f;
    f.id = "code.heavy_header";
    f.category = Category::CodeQuality;
    f.severity = Severity::Low;
    f.title = "Heavy header: " + util::filename_of(heavy_headers[i].first) +
              " (" + std::to_string(heavy_headers[i].second) + " #includes)";
    Location loc; loc.path = heavy_headers[i].first; f.location = loc;
    f.impact = "Pulled into many TUs — multiplies compile time across the build.";
    f.remediation = "Forward-declare, use the pImpl idiom, and move includes to .cpp.";
    f.metrics["includes"] = static_cast<double>(heavy_headers[i].second);
    add(std::move(f));
  }

  for (const auto& h : using_std_headers) {
    Finding f;
    f.id = "code.using_namespace_in_header";
    f.category = Category::CodeQuality;
    f.severity = Severity::Medium;
    f.title = "`using namespace std` in header " + util::filename_of(h);
    Location loc; loc.path = h; f.location = loc;
    f.impact = "Leaks the std namespace into every file that includes this header.";
    f.remediation = "Remove it from headers; qualify names or use it only in .cpp scope.";
    add(std::move(f));
  }

  if (total.endl > 0) {
    Finding f;
    f.id = "code.endl_flush";
    f.category = Category::CodeQuality;
    f.severity = total.endl > 50 ? Severity::Low : Severity::Info;
    f.title = std::to_string(total.endl) + " uses of std::endl (forces a stream flush)";
    if (have_first_endl) f.location = first_endl;
    f.impact = "std::endl flushes every call; in loops/logging this is a real cost.";
    f.remediation = "Prefer '\\n'; flush explicitly only when you must.";
    f.metrics["count"] = static_cast<double>(total.endl);
    add(std::move(f));
  }

  if (total.raw_new + total.raw_delete > 0) {
    Finding f;
    f.id = "code.raw_new_delete";
    f.category = Category::CodeQuality;
    f.severity = (total.raw_new + total.raw_delete) > 100 ? Severity::Medium
                                                          : Severity::Low;
    f.title = std::to_string(total.raw_new) + " raw new / " +
              std::to_string(total.raw_delete) + " raw delete";
    if (have_first_new) f.location = first_new;
    f.impact = "Manual memory management is a leak and double-free risk.";
    f.remediation = "Use std::unique_ptr / std::make_unique and containers.";
    f.metrics["new"] = static_cast<double>(total.raw_new);
    f.metrics["delete"] = static_cast<double>(total.raw_delete);
    add(std::move(f));
  }

  std::sort(deep_files.begin(), deep_files.end(),
            [](auto& a, auto& b) { return a.second > b.second; });
  for (size_t i = 0; i < deep_files.size() && i < 3; ++i) {
    Finding f;
    f.id = "code.deep_nesting";
    f.category = Category::CodeQuality;
    f.severity = Severity::Low;
    f.title = "Deep nesting (" + std::to_string(deep_files[i].second) +
              " levels) in " + util::filename_of(deep_files[i].first);
    Location loc; loc.path = deep_files[i].first; f.location = loc;
    f.impact = "Deeply nested code is hard to read, test, and optimize.";
    f.remediation = "Use early returns / guard clauses and extract helper functions.";
    f.metrics["depth"] = deep_files[i].second;
    add(std::move(f));
  }

  if (total.todos > 0) {
    Finding f;
    f.id = "code.tech_debt_markers";
    f.category = Category::CodeQuality;
    f.severity = total.todos > 100 ? Severity::Low : Severity::Info;
    f.title = std::to_string(total.todos) + " TODO/FIXME/HACK markers";
    f.impact = "Tracked-in-comments debt that tends to accumulate silently.";
    f.remediation = "Triage into the issue tracker; resolve or delete stale markers.";
    f.metrics["count"] = static_cast<double>(total.todos);
    add(std::move(f));
  }

  report.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - begin).count();
  return report;
}

}  // namespace pb::code
