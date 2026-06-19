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

PB_TEST_MAIN()
