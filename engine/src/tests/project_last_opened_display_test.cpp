#include "runtime/project/project_list.h"

#include <cstdio>
#include <string>

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

  expect_true("unset is em dash", formatProjectLastOpenedDisplay(0) == "—");
  expect_true("negative treated as unset",
              formatProjectLastOpenedDisplay(-1) == "—");

  const eastl::string stamped = formatProjectLastOpenedDisplay(1'700'000'000);
  expect_true("stamped length 19", stamped.size() == 19);
  expect_true(
      "stamped shape YYYY-MM-DD HH:MM:SS",
      stamped.size() == 19 && stamped[4] == '-' && stamped[7] == '-' &&
          stamped[10] == ' ' && stamped[13] == ':' && stamped[16] == ':');

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
