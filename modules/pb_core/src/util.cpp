#include "pb_core/util.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace pb::util {

std::optional<std::string> read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return std::nullopt;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::optional<std::uintmax_t> file_size(const std::string& path) {
  std::error_code ec;
  auto sz = fs::file_size(path, ec);
  if (ec) return std::nullopt;
  return sz;
}

bool path_exists(const std::string& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}

bool is_directory(const std::string& path) {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

std::string extension_of(const std::string& path) {
  std::string ext = fs::path(path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return ext;
}

std::string filename_of(const std::string& path) {
  return fs::path(path).filename().string();
}

std::vector<std::string> list_files(const std::string& root,
                                    const std::vector<std::string>& extensions) {
  std::vector<std::string> out;
  std::error_code ec;
  if (!fs::is_directory(root, ec)) {
    // A single file is a degenerate "tree".
    if (fs::is_regular_file(root, ec)) out.push_back(root);
    return out;
  }
  for (auto it = fs::recursive_directory_iterator(
           root, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    std::string p = it->path().string();
    if (extensions.empty()) {
      out.push_back(p);
    } else {
      std::string ext = extension_of(p);
      if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
        out.push_back(p);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::string iso8601_now() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_utc{};
#if defined(_WIN32)
  gmtime_s(&tm_utc, &t);
#else
  gmtime_r(&t, &tm_utc);
#endif
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
  return buf;
}

std::string human_bytes(double bytes) {
  const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  int u = 0;
  double v = bytes;
  while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
  char buf[32];
  if (u == 0)
    std::snprintf(buf, sizeof(buf), "%.0f %s", v, units[u]);
  else
    std::snprintf(buf, sizeof(buf), "%.1f %s", v, units[u]);
  return buf;
}

std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  std::stringstream ss(s);
  while (std::getline(ss, cur, ',')) {
    size_t a = cur.find_first_not_of(" \t");
    size_t b = cur.find_last_not_of(" \t");
    if (a == std::string::npos) continue;
    out.push_back(cur.substr(a, b - a + 1));
  }
  return out;
}

Args::Args(int argc, char** argv) {
  if (argc > 0) program_ = argv[0];
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a.rfind("--", 0) == 0) {
      std::string key = a.substr(2);
      // Support --key=value form.
      auto eq = key.find('=');
      if (eq != std::string::npos) {
        options_[key.substr(0, eq)] = key.substr(eq + 1);
        continue;
      }
      // Look ahead: if next token is a value (not another --flag), consume it.
      if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
        options_[key] = argv[++i];
      } else {
        flags_.push_back(key);
      }
    } else {
      positionals_.push_back(a);
    }
  }
}

std::optional<std::string> Args::get(const std::string& key) const {
  auto it = options_.find(key);
  if (it != options_.end()) return it->second;
  return std::nullopt;
}

std::string Args::get_or(const std::string& key, const std::string& def) const {
  auto v = get(key);
  return v ? *v : def;
}

bool Args::has(const std::string& key) const {
  if (options_.count(key)) return true;
  return std::find(flags_.begin(), flags_.end(), key) != flags_.end();
}

}  // namespace pb::util
