#include "runtime/project/project_relaunch.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  const auto args =
      buildProjectOpenArgv("C:/Games/Demo Project");
  expect_true("has exe placeholder then flags", args.size() >= 3);
  expect_true("flag is --project-root", args[1] == "--project-root");
  expect_true("path preserved", args[2] == "C:/Games/Demo Project");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
