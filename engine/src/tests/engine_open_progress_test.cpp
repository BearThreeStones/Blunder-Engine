#include "runtime/function/ui/engine_open_progress.h"

#include "runtime/core/boot_work_heartbeat.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

#include <chrono>
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
  using clock = std::chrono::steady_clock;

  engineOpenProgressResetForTest();

  expect_true("windowed editor mounts overlay",
              engineOpenProgressShouldMount(EngineHostMode::Editor, false, false));
  expect_true("headless editor skips overlay",
              !engineOpenProgressShouldMount(EngineHostMode::Editor, true, false));
  expect_true("cli/mcp headless skips overlay",
              !engineOpenProgressShouldMount(EngineHostMode::Editor, true, false));
  expect_true("windowed player skips overlay",
              !engineOpenProgressShouldMount(EngineHostMode::Player, false, false));
  expect_true("headless player skips overlay",
              !engineOpenProgressShouldMount(EngineHostMode::Player, true, false));
  expect_true("project manager skips overlay",
              !engineOpenProgressShouldMount(EngineHostMode::Editor, false, true));

  expect_true("cooking weight 40",
              engineOpenProgressStageWeight(
                  EngineOpenProgressStage::cookingAssets) == 40);
  expect_true("preparing weight 15",
              engineOpenProgressStageWeight(
                  EngineOpenProgressStage::preparingEditor) == 15);
  expect_true("starting weight 15",
              engineOpenProgressStageWeight(
                  EngineOpenProgressStage::startingEditor) == 15);
  expect_true("opening scene weight 20",
              engineOpenProgressStageWeight(
                  EngineOpenProgressStage::openingScene) == 20);
  expect_true("indexing weight 10",
              engineOpenProgressStageWeight(
                  EngineOpenProgressStage::indexingContent) == 10);
  expect_true(
      "weights sum 100",
      engineOpenProgressCompletedPercent(5) == 100);
  expect_true("cover stages already 70",
              engineOpenProgressCompletedPercent(3) == 70);
  expect_true("opening scene complete is 90",
              engineOpenProgressCompletedPercent(4) == 90);

  expect_true("title Opening editor",
              std::strcmp(engineOpenProgressTitle(), "Opening editor") == 0);
  expect_true("opening caption",
              std::strcmp(engineOpenProgressStageCaption(
                              EngineOpenProgressStage::openingScene),
                          "Opening scene") == 0);
  expect_true("indexing caption",
              std::strcmp(engineOpenProgressStageCaption(
                              EngineOpenProgressStage::indexingContent),
                          "Indexing content") == 0);
  expect_true("no global class names",
              std::strstr(engineOpenProgressStageCaption(
                              EngineOpenProgressStage::openingScene),
                          "global class") == nullptr);

  expect_true("no retry chrome", !engineOpenProgressOffersRetry());

  const auto t0 = clock::now();
  engineOpenProgressNoteSessionStart(t0);
  expect_true(
      "try show without window",
      engineOpenProgressTryShow(EngineHostMode::Editor, false, false, nullptr,
                                {}));
  expect_true("visible without OS window", engineOpenProgressIsVisible());
  expect_true("percent 70 at overlay show", engineOpenProgressPercent() == 70);
  expect_true("does not restart cook at 0", engineOpenProgressPercent() != 0);
  expect_true("caption Opening scene at show",
              std::strcmp(engineOpenProgressCaption(), "Opening scene") == 0);
  expect_true(
      "elapsed includes cover seconds",
      engineOpenProgressElapsedSeconds(t0 + std::chrono::seconds(7)) == 7);
  expect_true(
      "elapsed did not restart at overlay show",
      engineOpenProgressElapsedSeconds(t0 + std::chrono::seconds(7)) != 0);

  const int before_wait = engineOpenProgressPercent();
  expect_true(
      "percent does not lerp over wall time",
      engineOpenProgressPercent() == before_wait &&
          engineOpenProgressElapsedSeconds(t0 + std::chrono::seconds(30)) ==
              30 &&
          engineOpenProgressPercent() == 70);

  expect_true("still visible after host-gate query",
              engineOpenProgressIsVisible());

  engineOpenProgressCompleteOpeningScene();
  expect_true("percent 90 after opening scene",
              engineOpenProgressPercent() == 90);
  expect_true("caption Indexing content",
              std::strcmp(engineOpenProgressCaption(), "Indexing content") == 0);

  engineOpenProgressCompleteIndexing();
  expect_true("dismissed after index scan", !engineOpenProgressIsVisible());
  expect_true("percent 100 at dismiss", engineOpenProgressPercent() == 100);
  expect_true("consumed after dismiss", engineOpenProgressIsConsumed());
  expect_true("no retry after dismiss", !engineOpenProgressOffersRetry());
  expect_true(
      "dismiss does not wait Mesh/texture/thumb queues",
      !engineOpenProgressIsVisible());

  expect_true("second try-show after consume is skipped",
              !engineOpenProgressTryShow(EngineHostMode::Editor, false, false,
                                         nullptr, {}));
  expect_true("second openScene leaves overlay dismissed",
              !engineOpenProgressIsVisible());
  engineOpenProgressNotifyLaterOpenScene();
  expect_true("later openScene stays dismissed",
              !engineOpenProgressIsVisible());
  expect_true("later openScene stays consumed",
              engineOpenProgressIsConsumed());

  engineOpenProgressResetForTest();
  engineOpenProgressNoteSessionStart(t0);
  expect_true(
      "reshow after reset",
      engineOpenProgressTryShow(EngineHostMode::Editor, false, false, nullptr,
                                {}));
  expect_true("pump without window continues", engineOpenProgressPump());
  engineOpenProgressNoteCloseRequested();
  expect_true("close while overlay ends session",
              engineOpenProgressSessionEndedByClose());
  expect_true("close hides overlay", !engineOpenProgressIsVisible());
  expect_true("close has no Retry", !engineOpenProgressOffersRetry());
  expect_true("pump returns false after close", !engineOpenProgressPump());
  expect_true("heartbeat stays stopped after close",
              !bootWorkHeartbeatContinue());

  engineOpenProgressResetForTest();
  expect_true(
      "player try-show fails",
      !engineOpenProgressTryShow(EngineHostMode::Player, false, false, nullptr,
                                 {}));
  expect_true("player never visible", !engineOpenProgressIsVisible());
  expect_true(
      "project manager try-show fails",
      !engineOpenProgressTryShow(EngineHostMode::Editor, false, true, nullptr,
                                 {}));

  {
    Scene scene;
    constexpr int k_count = 300;
    for (int i = 0; i < k_count; ++i) {
      SceneEntityDefinition def;
      char name[16];
      std::snprintf(name, sizeof(name), "e%03d", i);
      def.name = name;
      def.mesh_virtual_path = "missing-mesh";
      if (i == 0 || i == k_count - 1) {
        def.has_camera = true;
      }
      scene.getEntities().push_back(eastl::move(def));
    }

    int beats = 0;
    setBootWorkHeartbeat([&beats]() {
      ++beats;
      if (beats >= 2) {
        g_runtime_global_context.requestQuit();
        return false;
      }
      return true;
    });

    SceneInstance instance;
    expect_true("instantiate aborts after first 256",
                !instance.instantiate(scene));
    expect_true("partial entity table is 256",
                instance.getEntityCount() == 256);
    expect_true("instantiate not completed", !instance.instantiateCompleted());
    expect_true("quit requested before attach",
                g_runtime_global_context.isQuitRequested());

    completeSceneDocumentInstantiate(nullptr, instance, scene, nullptr);
    const EntityId first = instance.findEntityByName("e000");
    expect_true("first entity exists", isValid(first));
    expect_true("complete skipped camera attach on abort",
                instance.getCamera(first) == nullptr);
    expect_true("missing tail entity not created",
                !isValid(instance.findEntityByName("e299")));

    setBootWorkHeartbeat({});
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
