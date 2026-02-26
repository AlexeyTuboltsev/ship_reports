#pragma once
// Minimal test runner — no external dependencies.
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

static std::vector<TestCase> &_tests() {
  static std::vector<TestCase> v;
  return v;
}

#define TEST(name)                             \
  static void _test_##name();                  \
  static int _reg_##name = ([]() {             \
    _tests().push_back({#name, _test_##name}); \
    return 0;                                  \
  })();                                        \
  static void _test_##name()

#define REQUIRE(expr)                                                         \
  do {                                                                        \
    if (!(expr)) {                                                            \
      std::fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
      throw std::string("assertion failed: " #expr);                          \
    }                                                                         \
  } while (0)

#define REQUIRE_EQ(a, b) REQUIRE((a) == (b))
#define REQUIRE_NEAR(a, b, eps) REQUIRE(std::abs((a) - (b)) < (eps))

inline int run_tests(int argc, char **argv) {
  const char *filter = (argc > 1) ? argv[1] : nullptr;
  int passed = 0, failed = 0;
  for (auto &t : _tests()) {
    if (filter && t.name.find(filter) == std::string::npos) continue;
    std::printf("[ RUN  ] %s\n", t.name.c_str());
    try {
      t.fn();
      std::printf("[ PASS ] %s\n", t.name.c_str());
      passed++;
    } catch (const std::string &msg) {
      std::printf("[ FAIL ] %s — %s\n", t.name.c_str(), msg.c_str());
      failed++;
    } catch (...) {
      std::printf("[ FAIL ] %s — unexpected exception\n", t.name.c_str());
      failed++;
    }
  }
  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
