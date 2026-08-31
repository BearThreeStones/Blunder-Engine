#include "runtime/function/ui/startup_cover.h"

#include <cstdio>
#include <cstring>

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

  expect_true("windowed editor mounts cover",
              startupCoverShouldMount(EngineHostMode::Editor, false));
  expect_true("headless editor skips cover",
              !startupCoverShouldMount(EngineHostMode::Editor, true));
  expect_true("windowed player skips cover",
              !startupCoverShouldMount(EngineHostMode::Player, false));
  expect_true("headless player skips cover",
              !startupCoverShouldMount(EngineHostMode::Player, true));

  expect_true("cooking stage name",
              std::strcmp(startupCoverStageName(StartupCoverPhase::cookingAssets),
                          "Cooking assets") == 0);
  expect_true(
      "preparing stage name",
      std::strcmp(startupCoverStageName(StartupCoverPhase::preparingEditor),
                  "Preparing editor") == 0);
  expect_true(
      "starting stage name",
      std::strcmp(startupCoverStageName(StartupCoverPhase::startingEditor),
                  "Starting editor") == 0);

  expect_true("cover inactive before begin", !startupCoverIsActive());

  startupCoverDismiss();
  startupCoverBegin(nullptr, eastl::string("Blunder Editor - Demo"));
  expect_true("null window never activates the cover", !startupCoverIsActive());
  expect_true("pump on inactive cover reports not-closed", startupCoverPump());
  startupCoverSetPhase(StartupCoverPhase::cookingAssets);
  expect_true("set phase on inactive cover stays inactive",
              !startupCoverIsActive());
  startupCoverDismiss();
  startupCoverDismiss();
  expect_true("dismiss is idempotent", !startupCoverIsActive());
  expect_true("pump after dismiss reports not-closed", startupCoverPump());

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
