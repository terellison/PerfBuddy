#pragma once
//
// Minimal header-only test harness. No third-party dependencies, in keeping
// with the rest of the analysis stack. One test binary per module; each binary
// registers TEST cases at static-init time and PB_TEST_MAIN() runs them all.
//
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace pb::test {

struct Case {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}

struct Registrar {
  Registrar(const std::string& name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
  }
};

inline int& failures() {
  static int f = 0;
  return f;
}

inline void report_failure(const char* file, int line, const std::string& expr) {
  std::fprintf(stderr, "  FAIL %s:%d: CHECK(%s)\n", file, line, expr.c_str());
  ++failures();
}

inline int run_all() {
  int total = static_cast<int>(registry().size());
  int failed_before;
  for (auto& c : registry()) {
    failed_before = failures();
    std::printf("[ RUN  ] %s\n", c.name.c_str());
    c.fn();
    if (failures() == failed_before) {
      std::printf("[  OK  ] %s\n", c.name.c_str());
    } else {
      std::printf("[ FAIL ] %s\n", c.name.c_str());
    }
  }
  std::printf("\n%d test case(s), %d assertion failure(s)\n", total, failures());
  return failures() == 0 ? 0 : 1;
}

}  // namespace pb::test

#define PB_TEST_CONCAT_(a, b) a##b
#define PB_TEST_CONCAT(a, b) PB_TEST_CONCAT_(a, b)

#define TEST(name)                                                          \
  static void PB_TEST_CONCAT(pb_test_fn_, __LINE__)();                      \
  static ::pb::test::Registrar PB_TEST_CONCAT(pb_test_reg_, __LINE__)(      \
      name, &PB_TEST_CONCAT(pb_test_fn_, __LINE__));                        \
  static void PB_TEST_CONCAT(pb_test_fn_, __LINE__)()

#define CHECK(cond)                                                         \
  do {                                                                      \
    if (!(cond)) ::pb::test::report_failure(__FILE__, __LINE__, #cond);     \
  } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

#define PB_TEST_MAIN()                                                      \
  int main() { return ::pb::test::run_all(); }
