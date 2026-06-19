#pragma once
//
// Small shared helpers used across modules: filesystem walking, time, and a
// minimal command-line flag parser. Kept tiny and dependency-free.
//
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pb::util {

// Read an entire file into a string. Returns nullopt if it cannot be opened.
std::optional<std::string> read_file(const std::string& path);

// File size in bytes, or nullopt if the path does not exist.
std::optional<std::uintmax_t> file_size(const std::string& path);

bool path_exists(const std::string& path);
bool is_directory(const std::string& path);

// Recursively list regular files under `root`. If `extensions` is non-empty,
// only files whose extension (lowercased, incl. dot, e.g. ".cpp") matches are
// returned.
std::vector<std::string> list_files(const std::string& root,
                                    const std::vector<std::string>& extensions = {});

// Lowercased file extension including the dot, e.g. ".cpp"; "" if none.
std::string extension_of(const std::string& path);
std::string filename_of(const std::string& path);

// Current UTC time as ISO-8601, e.g. "2026-06-19T20:14:00Z".
std::string iso8601_now();

// Human-readable byte size, e.g. "1.5 MiB".
std::string human_bytes(double bytes);

// Split a comma-separated list, trimming whitespace; empty entries dropped.
std::vector<std::string> split_csv(const std::string& s);

// A very small flag parser for the standalone module CLIs.
//   --key value     -> options["key"] = "value"
//   --flag          -> flags has "flag"
//   positional args -> positionals
class Args {
public:
  Args(int argc, char** argv);
  std::optional<std::string> get(const std::string& key) const;
  std::string get_or(const std::string& key, const std::string& def) const;
  bool has(const std::string& key) const;  // matches both --flag and --key value
  const std::vector<std::string>& positionals() const { return positionals_; }
  const std::string& program() const { return program_; }

private:
  std::string program_;
  std::map<std::string, std::string> options_;
  std::vector<std::string> flags_;
  std::vector<std::string> positionals_;
};

}  // namespace pb::util
