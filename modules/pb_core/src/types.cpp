#include "pb_core/types.hpp"

#include "pb_core/util.hpp"

namespace pb {

std::string to_string(Severity s) {
  switch (s) {
    case Severity::Info: return "info";
    case Severity::Low: return "low";
    case Severity::Medium: return "medium";
    case Severity::High: return "high";
    case Severity::Critical: return "critical";
  }
  return "info";
}

std::string to_string(Category c) {
  switch (c) {
    case Category::BinarySize: return "binary_size";
    case Category::Runtime: return "runtime";
    case Category::Memory: return "memory";
    case Category::CodeQuality: return "code_quality";
    case Category::Assets: return "assets";
  }
  return "code_quality";
}

std::string to_string(Engine e) {
  switch (e) {
    case Engine::Unknown: return "unknown";
    case Engine::Native: return "native";
    case Engine::Unreal: return "unreal";
  }
  return "unknown";
}

Severity severity_from_string(const std::string& s) {
  if (s == "critical") return Severity::Critical;
  if (s == "high") return Severity::High;
  if (s == "medium") return Severity::Medium;
  if (s == "low") return Severity::Low;
  return Severity::Info;
}

json::Value Location::to_json() const {
  json::Value v{json::Object{}};
  v.set("path", path);
  if (line) v.set("line", *line);
  if (!symbol.empty()) v.set("symbol", symbol);
  return v;
}

std::string Location::display() const {
  std::string out = path;
  if (line) out += ":" + std::to_string(*line);
  if (!symbol.empty()) out += " (" + symbol + ")";
  return out;
}

json::Value Finding::to_json() const {
  json::Value v{json::Object{}};
  v.set("id", id);
  v.set("title", title);
  v.set("description", description);
  v.set("severity", pb::to_string(severity));
  v.set("category", pb::to_string(category));
  if (location) v.set("location", location->to_json());
  v.set("impact", impact);
  v.set("remediation", remediation);
  json::Value m{json::Object{}};
  for (const auto& kv : metrics) m.set(kv.first, kv.second);
  v.set("metrics", m);
  return v;
}

int ModuleReport::count_at_least(Severity s) const {
  int n = 0;
  for (const auto& f : findings)
    if (static_cast<int>(f.severity) >= static_cast<int>(s)) ++n;
  return n;
}

json::Value ModuleReport::to_json() const {
  json::Value v{json::Object{}};
  v.set("module", module);
  v.set("version", version);
  v.set("target_label", target_label);
  if (error) v.set("error", *error);
  json::Value arr{json::Array{}};
  for (const auto& f : findings) arr.push_back(f.to_json());
  v.set("findings", arr);
  json::Value s{json::Object{}};
  for (const auto& kv : summary) s.set(kv.first, kv.second);
  v.set("summary", s);
  v.set("duration_ms", static_cast<double>(duration_ms));
  return v;
}

json::Value Report::to_json() const {
  json::Value v{json::Object{}};
  v.set("generated_at", generated_at);
  v.set("tool", "PerfBuddy");
  json::Value arr{json::Array{}};
  for (const auto& m : modules) arr.push_back(m.to_json());
  v.set("modules", arr);
  return v;
}

Engine Target::detect_engine(const Target& t) {
  if (t.engine != Engine::Unknown) return t.engine;
  // Unreal: a .uproject anywhere near the source root, or a Source/ +
  // Content/ pair, is the strongest signal.
  if (t.source_dir && util::is_directory(*t.source_dir)) {
    auto uprojects = util::list_files(*t.source_dir, {".uproject"});
    if (!uprojects.empty()) return Engine::Unreal;
    if (util::is_directory(*t.source_dir + "/Content") &&
        util::is_directory(*t.source_dir + "/Config")) {
      return Engine::Unreal;
    }
  }
  if (t.data_dir && util::is_directory(*t.data_dir)) {
    if (!util::list_files(*t.data_dir, {".uasset", ".pak"}).empty())
      return Engine::Unreal;
  }
  // If there's a source tree or executable at all, treat it as native C/C++.
  if (t.source_dir || t.executable) return Engine::Native;
  return Engine::Unknown;
}

}  // namespace pb
