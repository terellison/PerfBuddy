#include "pb_test.hpp"
#include "test_fixture.hpp"

#include "pb_code/analyzer.hpp"

using namespace pb;
using pb::test::TempDir;

TEST("pb_code: does not support a target with no source_dir") {
  code::CodeAnalyzer a;
  Target t;
  CHECK(!a.supports(t));
}

TEST("pb_code: does not support a nonexistent source_dir") {
  code::CodeAnalyzer a;
  Target t;
  t.source_dir = "/this/path/does/not/exist";
  CHECK(!a.supports(t));
}

TEST("pb_code: supports an existing source_dir") {
  code::CodeAnalyzer a;
  TempDir dir;
  Target t;
  t.source_dir = dir.str();
  CHECK(a.supports(t));
}

TEST("pb_code: analyze scans source files without error") {
  code::CodeAnalyzer a;
  TempDir dir;
  dir.write("game.cpp", "int main() {\n  // TODO: fix this\n  return 0;\n}\n");
  Target t;
  t.source_dir = dir.str();
  ModuleReport r = a.analyze(t);
  CHECK(!r.error.has_value());
  CHECK_EQ(r.module, "pb_code");
  CHECK(r.summary.count("lines") > 0);
}

TEST("pb_code: flags raw new/delete usage as a finding") {
  code::CodeAnalyzer a;
  TempDir dir;
  dir.write("leaky.cpp", "void f() { int* p = new int[100]; (void)p; }\n");
  Target t;
  t.source_dir = dir.str();
  ModuleReport r = a.analyze(t);
  CHECK(!r.error.has_value());
  const Finding* found = nullptr;
  for (const auto& f : r.findings)
    if (f.id == "code.raw_new_delete") found = &f;
  CHECK(found != nullptr);
  if (found) CHECK_EQ(found->metrics.at("new"), 1.0);
}

TEST("pb_code: a rules file can disable a finding") {
  code::CodeAnalyzer a;
  TempDir dir;
  dir.write("leaky.cpp", "void f() { int* p = new int[100]; (void)p; }\n");
  auto rules = dir.write("rules.ini", "[code.raw_new_delete]\nseverity = none\n");
  Target t;
  t.source_dir = dir.str();
  t.rules_file = rules;
  ModuleReport r = a.analyze(t);
  CHECK(!r.error.has_value());
  for (const auto& f : r.findings) CHECK(f.id != "code.raw_new_delete");
}

TEST("pb_code: a rules file can override a finding's severity") {
  code::CodeAnalyzer a;
  TempDir dir;
  dir.write("leaky.cpp", "void f() { int* p = new int[100]; (void)p; }\n");
  auto rules = dir.write("rules.ini", "[code.raw_new_delete]\nseverity = critical\n");
  Target t;
  t.source_dir = dir.str();
  t.rules_file = rules;
  ModuleReport r = a.analyze(t);
  const Finding* found = nullptr;
  for (const auto& f : r.findings)
    if (f.id == "code.raw_new_delete") found = &f;
  CHECK(found != nullptr);
  if (found) CHECK(found->severity == Severity::Critical);
}

TEST("pb_code: an unreadable rules file degrades gracefully") {
  code::CodeAnalyzer a;
  TempDir dir;
  dir.write("game.cpp", "int main() { return 0; }\n");
  Target t;
  t.source_dir = dir.str();
  t.rules_file = dir.str() + "/does_not_exist.ini";
  ModuleReport r = a.analyze(t);
  CHECK(!r.error.has_value());
  bool warned = false;
  for (const auto& f : r.findings)
    if (f.id == "code.rules_file_error") warned = true;
  CHECK(warned);
}

PB_TEST_MAIN()
