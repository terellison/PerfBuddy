#include "pb_unreal/analyzer.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>

#include "pb_core/util.hpp"

namespace pb::unreal {

namespace {

bool contains(const std::string& hay, const std::string& needle) {
  return hay.find(needle) != std::string::npos;
}

std::string canonical(const std::string& p) {
  std::error_code ec;
  auto c = std::filesystem::weakly_canonical(p, ec);
  return ec ? p : c.string();
}

// Roots to look in: source dir and data dir (either may hold Content/).
// Deduplicated so that --src and --data pointing at the same tree don't cause
// every asset/config to be reported twice.
std::vector<std::string> roots(const Target& t) {
  std::vector<std::string> r;
  std::vector<std::string> seen;
  auto add = [&](const std::optional<std::string>& d) {
    if (!d || !util::is_directory(*d)) return;
    std::string c = canonical(*d);
    if (std::find(seen.begin(), seen.end(), c) != seen.end()) return;
    seen.push_back(c);
    r.push_back(*d);
  };
  add(t.source_dir);
  add(t.data_dir);
  return r;
}

// Collect files of the given extensions across roots, deduped by real path.
std::vector<std::string> collect(const std::vector<std::string>& roots_in,
                                 const std::vector<std::string>& exts) {
  std::vector<std::string> out;
  std::vector<std::string> seen;
  for (const auto& root : roots_in) {
    for (const auto& f : util::list_files(root, exts)) {
      std::string c = canonical(f);
      if (std::find(seen.begin(), seen.end(), c) != seen.end()) continue;
      seen.push_back(c);
      out.push_back(f);
    }
  }
  return out;
}

}  // namespace

bool UnrealAnalyzer::supports(const Target& t) const {
  if (t.engine == Engine::Unreal) return true;
  for (const auto& root : roots(t)) {
    if (!util::list_files(root, {".uproject"}).empty()) return true;
    if (!util::list_files(root, {".uasset", ".pak", ".umap"}).empty()) return true;
  }
  return false;
}

ModuleReport UnrealAnalyzer::analyze(const Target& t) const {
  auto begin = std::chrono::steady_clock::now();
  ModuleReport report;
  report.module = name();
  report.version = "0.1.0";
  report.target_label = t.label;

  auto add = [&](Finding f) { report.findings.push_back(std::move(f)); };
  auto search_roots = roots(t);

  // --- Project detection ---
  std::string uproject;
  for (const auto& root : search_roots) {
    auto ups = util::list_files(root, {".uproject"});
    if (!ups.empty()) { uproject = ups.front(); break; }
  }
  if (!uproject.empty()) {
    Finding f;
    f.id = "unreal.project";
    f.category = Category::Assets;
    f.severity = Severity::Info;
    f.title = "Unreal project detected: " + util::filename_of(uproject);
    Location loc; loc.path = uproject; f.location = loc;
    f.impact = "Enables Unreal-specific asset, config, and C++ analysis.";
    f.remediation = "—";
    add(std::move(f));
  }

  // --- Cooked asset sizing ---
  std::vector<std::pair<std::string, std::uintmax_t>> assets;
  std::uintmax_t total_assets = 0;
  for (const auto& f : collect(search_roots, {".uasset", ".uexp", ".pak", ".umap"})) {
    auto sz = util::file_size(f);
    if (!sz) continue;
    assets.emplace_back(f, *sz);
    total_assets += *sz;
  }
  if (!assets.empty()) {
    report.summary["asset_files"] = static_cast<double>(assets.size());
    report.summary["asset_bytes"] = static_cast<double>(total_assets);
    {
      Finding f;
      f.id = "unreal.content_size";
      f.category = Category::Assets;
      f.severity = Severity::Info;
      f.title = "Cooked content: " + util::human_bytes(static_cast<double>(total_assets)) +
                " across " + std::to_string(assets.size()) + " files";
      f.impact = "Dominates package/download size and disk footprint.";
      f.remediation = "Audit the largest assets below; compress and trim unused content.";
      f.metrics["asset_bytes"] = static_cast<double>(total_assets);
      add(std::move(f));
    }
    std::sort(assets.begin(), assets.end(),
              [](auto& a, auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < assets.size() && i < 6; ++i) {
      double mb = static_cast<double>(assets[i].second) / (1024.0 * 1024.0);
      Finding f;
      f.id = "unreal.large_asset";
      f.category = Category::Assets;
      f.severity = mb > 100 ? Severity::High : mb > 25 ? Severity::Medium : Severity::Low;
      f.title = "Large asset " + util::filename_of(assets[i].first) + " = " +
                util::human_bytes(static_cast<double>(assets[i].second));
      Location loc; loc.path = assets[i].first; f.location = loc;
      f.impact = "Heavy assets inflate package size, memory, and load times.";
      f.remediation = "Reduce texture resolution / compression, LODs for meshes, "
                      "and audio bitrate; split or stream very large packages.";
      f.metrics["bytes"] = static_cast<double>(assets[i].second);
      add(std::move(f));
    }
  }

  // --- Config scan (Config/*.ini) ---
  struct Rule { const char* needle; Severity sev; const char* why; const char* fix; };
  const Rule rules[] = {
      {"r.TextureStreaming=False", Severity::High,
       "Texture streaming disabled — all textures stay resident in memory.",
       "Set r.TextureStreaming=True and tune r.Streaming.PoolSize."},
      {"r.TextureStreaming=0", Severity::High,
       "Texture streaming disabled — all textures stay resident in memory.",
       "Re-enable texture streaming."},
      {"bCompressPackages=False", Severity::Medium,
       "Package compression off — larger on-disk and download size.",
       "Enable package compression for shipping builds."},
      {"r.ShadowQuality=5", Severity::Low,
       "Maximum shadow quality set as default — expensive on lower-end hardware.",
       "Expose as a scalability setting rather than forcing the max."},
      {"bUseFixedFrameRate=True", Severity::Info,
       "Fixed frame rate is enabled — intended only for capture/determinism.",
       "Confirm this is intentional for shipping."},
      {"r.Streaming.PoolSize=0", Severity::Medium,
       "Streaming pool size 0 disables the texture streaming budget.",
       "Set a sensible pool size for the target platform."},
  };
  std::vector<std::string> ini_files = collect(search_roots, {".ini"});
  for (const auto& path : ini_files) {
    auto c = util::read_file(path);
    if (!c) continue;
    for (const auto& r : rules) {
      if (contains(*c, r.needle)) {
        Finding f;
        f.id = "unreal.config";
        f.category = Category::Assets;
        f.severity = r.sev;
        f.title = std::string("Config: ") + r.needle + " in " + util::filename_of(path);
        Location loc; loc.path = path; f.location = loc;
        f.impact = r.why;
        f.remediation = r.fix;
        add(std::move(f));
      }
    }
  }

  // --- Unreal C++ heuristics ---
  long long tick_enabled = 0;
  std::vector<std::string> tick_with_getworld;
  std::vector<std::string> getallactors;
  if (t.source_dir && util::is_directory(*t.source_dir)) {
    for (const auto& path : util::list_files(*t.source_dir, {".cpp", ".h", ".hpp"})) {
      auto c = util::read_file(path);
      if (!c) continue;
      const std::string& s = *c;
      if (contains(s, "bCanEverTick = true") || contains(s, "bCanEverTick=true"))
        ++tick_enabled;
      bool has_tick = contains(s, "::Tick(") || contains(s, "void Tick(");
      if (has_tick && (contains(s, "GetWorld()") || contains(s, "Cast<") ||
                       contains(s, "FindComponentByClass")))
        tick_with_getworld.push_back(path);
      if (contains(s, "GetAllActorsOfClass") || contains(s, "GetAllActorsWithTag"))
        getallactors.push_back(path);
    }
  }
  if (tick_enabled > 0) {
    Finding f;
    f.id = "unreal.tick_count";
    f.category = Category::Runtime;
    f.severity = tick_enabled > 20 ? Severity::Medium : Severity::Low;
    f.title = std::to_string(tick_enabled) + " actor/component types enable per-frame Tick";
    f.impact = "Every ticking object costs CPU each frame and limits scalability.";
    f.remediation = "Disable Tick where possible; use timers/events or tick intervals.";
    f.metrics["tick_enabled"] = static_cast<double>(tick_enabled);
    add(std::move(f));
  }
  for (size_t i = 0; i < tick_with_getworld.size() && i < 5; ++i) {
    Finding f;
    f.id = "unreal.work_in_tick";
    f.category = Category::Runtime;
    f.severity = Severity::Medium;
    f.title = "Expensive call in Tick: " + util::filename_of(tick_with_getworld[i]);
    Location loc; loc.path = tick_with_getworld[i]; f.location = loc;
    f.impact = "GetWorld()/Cast<>/component lookups every frame add up fast.";
    f.remediation = "Cache results outside Tick; cast/lookup once and store the pointer.";
    add(std::move(f));
  }
  for (size_t i = 0; i < getallactors.size() && i < 5; ++i) {
    Finding f;
    f.id = "unreal.get_all_actors";
    f.category = Category::Runtime;
    f.severity = Severity::High;
    f.title = "GetAllActorsOfClass in " + util::filename_of(getallactors[i]);
    Location loc; loc.path = getallactors[i]; f.location = loc;
    f.impact = "Iterates every actor in the level — O(N) and very slow if called often.";
    f.remediation = "Maintain a cached registry/array of the actors you need instead.";
    add(std::move(f));
  }

  if (report.findings.empty()) {
    report.error = "no Unreal project, assets, config, or source found to analyze";
  }
  report.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - begin).count();
  return report;
}

}  // namespace pb::unreal
