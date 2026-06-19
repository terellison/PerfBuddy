#include "pb_test.hpp"
#include "test_fixture.hpp"

#include "pb_core/json.hpp"
#include "pb_core/types.hpp"
#include "pb_core/util.hpp"

using namespace pb;
using pb::test::TempDir;

// ---- json ----

TEST("json: scalars round-trip through parse(dump())") {
  json::Value v = 42;
  CHECK_EQ(json::Value::parse(v.dump(-1)).as_number(), 42.0);
  json::Value s = std::string("hello");
  CHECK_EQ(json::Value::parse(s.dump(-1)).as_string(), "hello");
  json::Value b = true;
  CHECK_EQ(json::Value::parse(b.dump(-1)).as_bool(), true);
}

TEST("json: object set/contains/operator[]") {
  json::Value obj = json::Object{};
  obj.set("a", 1);
  obj.set("b", std::string("x"));
  CHECK(obj.contains("a"));
  CHECK(!obj.contains("missing"));
  CHECK_EQ(obj["a"].as_number(), 1.0);
  CHECK_EQ(obj["b"].as_string(), "x");
  CHECK(obj["missing"].is_null());
}

TEST("json: array push_back and round-trip") {
  json::Value arr = json::Array{};
  arr.push_back(1);
  arr.push_back(2);
  arr.push_back(3);
  CHECK_EQ(arr.as_array().size(), 3u);
  auto parsed = json::Value::parse(arr.dump(-1));
  CHECK_EQ(parsed.as_array().size(), 3u);
  CHECK_EQ(parsed.as_array()[1].as_number(), 2.0);
}

TEST("json: parse throws on malformed input") {
  bool threw = false;
  try {
    json::Value::parse("{not valid json");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  CHECK(threw);
}

TEST("json: nested object/array survives a round trip") {
  json::Value root = json::Object{};
  json::Value items = json::Array{};
  json::Value item = json::Object{};
  item.set("name", std::string("finding"));
  item.set("score", 3.5);
  items.push_back(item);
  root.set("items", items);

  auto parsed = json::Value::parse(root.dump(2));
  CHECK(parsed.contains("items"));
  CHECK_EQ(parsed["items"].as_array().size(), 1u);
  CHECK_EQ(parsed["items"].as_array()[0]["name"].as_string(), "finding");
  CHECK_EQ(parsed["items"].as_array()[0]["score"].as_number(), 3.5);
}

// ---- types ----

TEST("types: Severity to_string/from_string round-trip") {
  for (auto s : {Severity::Info, Severity::Low, Severity::Medium, Severity::High,
                 Severity::Critical}) {
    CHECK(severity_from_string(to_string(s)) == s);
  }
}

TEST("types: detect_engine finds Unreal via .uproject under source_dir") {
  TempDir dir;
  dir.write("MyGame.uproject", "{}");
  Target t;
  t.source_dir = dir.str();
  CHECK(Target::detect_engine(t) == Engine::Unreal);
}

TEST("types: detect_engine finds Unreal via Content/+Config/ pair") {
  TempDir dir;
  dir.mkdir("Content");
  dir.mkdir("Config");
  Target t;
  t.source_dir = dir.str();
  CHECK(Target::detect_engine(t) == Engine::Unreal);
}

TEST("types: detect_engine finds Unreal via .uasset in data_dir") {
  TempDir dir;
  dir.write("Stuff.uasset", "x");
  Target t;
  t.data_dir = dir.str();
  CHECK(Target::detect_engine(t) == Engine::Unreal);
}

TEST("types: detect_engine falls back to Native for a plain source tree") {
  TempDir dir;
  dir.write("main.cpp", "int main(){}");
  Target t;
  t.source_dir = dir.str();
  CHECK(Target::detect_engine(t) == Engine::Native);
}

TEST("types: detect_engine is Unknown with no source/exe/data") {
  Target t;
  CHECK(Target::detect_engine(t) == Engine::Unknown);
}

TEST("types: detect_engine respects an already-set engine") {
  Target t;
  t.engine = Engine::Native;
  t.data_dir = "/nonexistent/has/.uasset/markers";
  CHECK(Target::detect_engine(t) == Engine::Native);
}

TEST("types: ModuleReport::count_at_least filters by severity") {
  ModuleReport r;
  Finding low; low.severity = Severity::Low;
  Finding high; high.severity = Severity::High;
  Finding crit; crit.severity = Severity::Critical;
  r.findings = {low, high, crit};
  CHECK_EQ(r.count_at_least(Severity::High), 2);
  CHECK_EQ(r.count_at_least(Severity::Critical), 1);
  CHECK_EQ(r.count_at_least(Severity::Info), 3);
}

// ---- util ----

TEST("util: path_exists/is_directory/file_size") {
  TempDir dir;
  auto f = dir.write("a.txt", "hello");
  CHECK(util::path_exists(f));
  CHECK(!util::is_directory(f));
  CHECK(util::is_directory(dir.str()));
  CHECK_EQ(util::file_size(f).value(), 5u);
  CHECK(!util::path_exists(dir.str() + "/does_not_exist"));
}

TEST("util: extension_of and filename_of") {
  CHECK_EQ(util::extension_of("/a/b/Texture.uasset"), ".uasset");
  CHECK_EQ(util::filename_of("/a/b/Texture.uasset"), "Texture.uasset");
  CHECK_EQ(util::extension_of("/a/b/no_ext"), "");
}

TEST("util: list_files filters by extension and is recursive") {
  TempDir dir;
  dir.write("a.cpp", "");
  dir.write("b.hpp", "");
  dir.write("sub/c.cpp", "");
  dir.write("notes.txt", "");
  auto cpp = util::list_files(dir.str(), {".cpp"});
  CHECK_EQ(cpp.size(), 2u);
  auto all = util::list_files(dir.str());
  CHECK_EQ(all.size(), 4u);
}

TEST("util: human_bytes formats across units") {
  CHECK_EQ(util::human_bytes(512), "512 B");
  CHECK(util::human_bytes(1536).find("KiB") != std::string::npos);
  CHECK(util::human_bytes(200.0 * 1024 * 1024).find("MiB") != std::string::npos);
}

TEST("util: split_csv trims and drops empties") {
  auto v = util::split_csv("a, b ,,c");
  CHECK_EQ(v.size(), 3u);
  CHECK_EQ(v[0], "a");
  CHECK_EQ(v[1], "b");
  CHECK_EQ(v[2], "c");
}

TEST("util: Args parses flags, key=value, key value, and positionals") {
  const char* argv[] = {"prog", "--verbose", "--src=foo", "--data", "bar", "pos1"};
  util::Args args(static_cast<int>(sizeof(argv) / sizeof(argv[0])),
                  const_cast<char**>(argv));
  CHECK(args.has("verbose"));
  CHECK_EQ(args.get_or("src", ""), "foo");
  CHECK_EQ(args.get_or("data", ""), "bar");
  CHECK(!args.has("missing"));
  CHECK_EQ(args.positionals().size(), 1u);
  CHECK_EQ(args.positionals()[0], "pos1");
}

PB_TEST_MAIN()
