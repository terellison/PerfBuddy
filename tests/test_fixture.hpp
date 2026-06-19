#pragma once
//
// Self-cleaning temp directory for tests that need real files on disk
// (util::list_files, Analyzer::supports/analyze, etc).
//
#include <filesystem>
#include <fstream>
#include <string>

namespace pb::test {

class TempDir {
 public:
  TempDir() {
    auto base = std::filesystem::temp_directory_path();
    path_ = base / ("pbtest-" + std::to_string(reinterpret_cast<uintptr_t>(this)));
    std::filesystem::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  const std::filesystem::path& path() const { return path_; }
  std::string str() const { return path_.string(); }

  // Writes a text file (creating parent dirs) and returns its path.
  std::string write(const std::string& rel, const std::string& content) const {
    auto p = path_ / rel;
    std::filesystem::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out << content;
    return p.string();
  }

  // Creates a sparse file of the given size (fast — no actual I/O of `bytes`
  // zero bytes) for testing size-threshold heuristics.
  std::string write_sparse(const std::string& rel, std::uintmax_t bytes) const {
    auto p = path_ / rel;
    std::filesystem::create_directories(p.parent_path());
    { std::ofstream out(p, std::ios::binary); }
    std::filesystem::resize_file(p, bytes);
    return p.string();
  }

  std::string mkdir(const std::string& rel) const {
    auto p = path_ / rel;
    std::filesystem::create_directories(p);
    return p.string();
  }

 private:
  std::filesystem::path path_;
};

}  // namespace pb::test
