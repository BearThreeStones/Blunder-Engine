#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/render/player_authorship_input.h"

#include <cstdio>

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

  expect_true("editor allows authorship input",
              playerAuthorshipInputEnabled(EngineHostMode::Editor));
  expect_true("player blocks authorship input",
              !playerAuthorshipInputEnabled(EngineHostMode::Player));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("player_authorship_input_test: all passed\n");
  return 0;
}
