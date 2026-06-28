#pragma once
//
// Shared rule-configuration mechanism: an .editorconfig-style file that lets
// users disable individual Finding ids or override their severity, decoupled
// from whichever module owns the rule. Any analyzer can opt in by loading
// Target::rules_file through RuleSettings.
//
#include <map>
#include <optional>
#include <string>

#include "pb_core/types.hpp"

namespace pb {

class RuleSettings {
public:
  // Parses a file shaped like:
  //
  //   [code.endl_flush]
  //   severity = none        ; disables the rule entirely
  //
  //   [code.oversized_tu]
  //   severity = high        ; info | low | medium | high | critical
  //
  // Lines starting with '#' or ';' are comments; blank lines are ignored.
  // Section names are Finding::id values. Unknown keys are ignored so the
  // format can grow without breaking older files. If the file cannot be
  // opened, the returned RuleSettings has failed() set and leaves every rule
  // at its module-supplied default.
  static RuleSettings load(const std::string& path);

  bool failed() const { return failed_; }
  const std::string& error() const { return error_; }

  // False only if the rule's severity was explicitly set to a disabling
  // value (none/disabled/off) or `enabled = false`.
  bool is_enabled(const std::string& rule_id) const;

  // `default_severity` unless the file overrides this rule's severity.
  Severity effective_severity(const std::string& rule_id, Severity default_severity) const;

private:
  struct Rule {
    bool enabled = true;
    std::optional<Severity> severity;
  };
  std::map<std::string, Rule> rules_;
  bool failed_ = false;
  std::string error_;
};

}  // namespace pb
