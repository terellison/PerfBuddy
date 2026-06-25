#include "pb_core/ruleset.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace pb {

namespace {

std::string trim(const std::string& s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool is_falsey(const std::string& v) {
  return v == "false" || v == "0" || v == "no" || v == "off";
}

bool is_disable_token(const std::string& v) {
  return v == "none" || v == "disabled" || is_falsey(v);
}

}  // namespace

RuleSettings RuleSettings::load(const std::string& path) {
  RuleSettings settings;
  std::ifstream in(path);
  if (!in) {
    settings.failed_ = true;
    settings.error_ = "could not open rules file: " + path;
    return settings;
  }

  std::string line, section;
  while (std::getline(in, line)) {
    // '#' or ';' starts a comment, whether the line is comment-only or the
    // marker trails real content (e.g. "severity = none  ; why").
    size_t comment_pos = line.find_first_of("#;");
    if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
    std::string l = trim(line);
    if (l.empty()) continue;
    if (l.front() == '[' && l.back() == ']') {
      section = trim(l.substr(1, l.size() - 2));
      continue;
    }
    if (section.empty()) continue;
    size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    std::string key = lower(trim(l.substr(0, eq)));
    std::string value = lower(trim(l.substr(eq + 1)));
    Rule& rule = settings.rules_[section];
    if (key == "severity") {
      if (is_disable_token(value)) {
        rule.enabled = false;
      } else {
        rule.enabled = true;
        rule.severity = severity_from_string(value);
      }
    } else if (key == "enabled") {
      rule.enabled = !is_falsey(value);
    }
  }
  return settings;
}

bool RuleSettings::is_enabled(const std::string& rule_id) const {
  auto it = rules_.find(rule_id);
  return it == rules_.end() || it->second.enabled;
}

Severity RuleSettings::effective_severity(const std::string& rule_id,
                                          Severity default_severity) const {
  auto it = rules_.find(rule_id);
  if (it == rules_.end() || !it->second.severity) return default_severity;
  return *it->second.severity;
}

}  // namespace pb
