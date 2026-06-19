#include "pb_test.hpp"
#include "test_fixture.hpp"

#include "pb_app/app.hpp"

using namespace pb;
using pb::test::TempDir;

TEST("pb_app: list_modules returns all five analyzers") {
  auto mods = pb::app::list_modules();
  CHECK_EQ(mods.size(), 5u);
}

TEST("pb_app: run_analysis only runs analyzers that support the target") {
  TempDir dir;
  dir.write("main.cpp", "int main(){}");
  Target t;
  t.source_dir = dir.str();
  Report r = pb::app::run_analysis(t, {}, /*parallel=*/false);

  bool ran_code = false, ran_unreal = false, ran_binary = false;
  for (const auto& m : r.modules) {
    if (m.module == "pb_code") ran_code = true;
    if (m.module == "pb_unreal") ran_unreal = true;
    if (m.module == "pb_binary") ran_binary = true;
  }
  CHECK(ran_code);     // has a source_dir with .cpp
  CHECK(!ran_unreal);  // no .uproject / cooked assets
  CHECK(!ran_binary);  // no executable
}

TEST("pb_app: run_analysis honors the `only` module filter") {
  TempDir dir;
  dir.write("main.cpp", "int main(){}");
  Target t;
  t.source_dir = dir.str();
  Report r = pb::app::run_analysis(t, {"pb_code"}, /*parallel=*/false);
  CHECK_EQ(r.modules.size(), 1u);
  CHECK_EQ(r.modules[0].module, "pb_code");
}

TEST("pb_app: run_analysis auto-detects Unreal and routes pb_unreal in") {
  TempDir dir;
  dir.write("MyGame.uproject", "{}");
  Target t;
  t.source_dir = dir.str();
  Report r = pb::app::run_analysis(t, {}, /*parallel=*/false);
  bool ran_unreal = false;
  for (const auto& m : r.modules) ran_unreal |= (m.module == "pb_unreal");
  CHECK(ran_unreal);
}

TEST("pb_app: parallel and sequential runs produce the same module set") {
  TempDir dir;
  dir.write("main.cpp", "int main(){}");
  Target t;
  t.source_dir = dir.str();
  Report seq = pb::app::run_analysis(t, {}, /*parallel=*/false);
  Report par = pb::app::run_analysis(t, {}, /*parallel=*/true);
  CHECK_EQ(seq.modules.size(), par.modules.size());
}

PB_TEST_MAIN()
