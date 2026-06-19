#include "pb_test.hpp"
#include "test_fixture.hpp"

#include "pb_unreal/analyzer.hpp"

using namespace pb;
using pb::test::TempDir;

TEST("pb_unreal: does not support a plain native source tree") {
  unreal::UnrealAnalyzer a;
  TempDir dir;
  dir.write("main.cpp", "int main(){}");
  Target t;
  t.source_dir = dir.str();
  CHECK(!a.supports(t));
}

TEST("pb_unreal: supports a tree with a .uproject") {
  unreal::UnrealAnalyzer a;
  TempDir dir;
  dir.write("MyGame.uproject", "{}");
  Target t;
  t.source_dir = dir.str();
  CHECK(a.supports(t));
}

TEST("pb_unreal: supports a data_dir containing cooked .uasset files") {
  unreal::UnrealAnalyzer a;
  TempDir dir;
  dir.write("Foo.uasset", "x");
  Target t;
  t.data_dir = dir.str();
  CHECK(a.supports(t));
}

TEST("pb_unreal: reports the .uproject as detected") {
  unreal::UnrealAnalyzer a;
  TempDir dir;
  dir.write("MyGame.uproject", "{}");
  Target t;
  t.source_dir = dir.str();
  ModuleReport r = a.analyze(t);
  bool found = false;
  for (const auto& f : r.findings) found |= (f.id == "unreal.project");
  CHECK(found);
}

TEST("pb_unreal: severity scales with asset size (Low/Medium/High thresholds)") {
  unreal::UnrealAnalyzer a;
  TempDir dir;
  dir.write_sparse("Content/Small.uasset", 1 * 1024 * 1024);          // 1 MiB -> Low
  dir.write_sparse("Content/Medium.uasset", 30 * 1024 * 1024);        // 30 MiB -> Medium
  dir.write_sparse("Content/Big.uasset", 110 * 1024 * 1024);          // 110 MiB -> High
  Target t;
  t.data_dir = dir.str();
  ModuleReport r = a.analyze(t);

  CHECK_EQ(r.summary.at("asset_files"), 3.0);

  Severity small_sev = Severity::Info, medium_sev = Severity::Info, big_sev = Severity::Info;
  for (const auto& f : r.findings) {
    if (f.id != "unreal.large_asset" || !f.location) continue;
    if (f.location->path.find("Small.uasset") != std::string::npos) small_sev = f.severity;
    if (f.location->path.find("Medium.uasset") != std::string::npos) medium_sev = f.severity;
    if (f.location->path.find("Big.uasset") != std::string::npos) big_sev = f.severity;
  }
  CHECK(small_sev == Severity::Low);
  CHECK(medium_sev == Severity::Medium);
  CHECK(big_sev == Severity::High);
}

TEST("pb_unreal: dedupes assets visible through both source_dir and data_dir") {
  unreal::UnrealAnalyzer a;
  TempDir dir;
  dir.write_sparse("Content/Shared.uasset", 1024);
  Target t;
  t.source_dir = dir.str();
  t.data_dir = dir.str();  // same tree from both sides
  ModuleReport r = a.analyze(t);
  CHECK_EQ(r.summary.at("asset_files"), 1.0);
}

PB_TEST_MAIN()
