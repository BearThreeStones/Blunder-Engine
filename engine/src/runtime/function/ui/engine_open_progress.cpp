#include "runtime/function/ui/engine_open_progress.h"

#include "runtime/core/boot_work_heartbeat.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {
namespace {

constexpr int k_stage_weights[] = {40, 15, 15, 20, 10};
constexpr int k_stage_count =
    static_cast<int>(sizeof(k_stage_weights) / sizeof(k_stage_weights[0]));
constexpr int k_cover_completed_stages = 3;

bool g_visible{false};
bool g_consumed{false};
bool g_session_ended_by_close{false};
bool g_session_start_set{false};
int g_completed_stages{0};
std::chrono::steady_clock::time_point g_session_start{};
std::chrono::steady_clock::time_point g_last_present{};
int g_last_presented_elapsed{-1};
WindowSystem* g_window{nullptr};
EngineOpenProgressPresent g_present;

void presentNow() {
  g_last_present = std::chrono::steady_clock::now();
  g_last_presented_elapsed = engineOpenProgressElapsedSeconds(g_last_present);
  if (g_present) {
    g_present();
  }
}

void unbindHost() {
  g_window = nullptr;
  g_present = {};
  setBootWorkHeartbeat({});
}

void hideAndUnbind() {
  const bool was_visible = g_visible;
  g_visible = false;
  if (was_visible && g_present) {
    g_present();
  }
  unbindHost();
}

void hideAndUnbind() {
  const bool was_visible = g_visible;
  g_visible = false;
  if (was_visible && g_present) {
    g_present();
  }
  unbindHost();
}

}  // namespace

bool engineOpenProgressShouldMount(EngineHostMode mode, bool headless,
                                   bool project_manager) {
  return hostMountsEditorShell(mode, headless) && !project_manager;
}

int engineOpenProgressStageWeight(EngineOpenProgressStage stage) {
  const int index = static_cast<int>(stage);
  if (index < 0 || index >= k_stage_count) {
    return 0;
  }
  return k_stage_weights[index];
}

int engineOpenProgressCompletedPercent(int completed_stage_count) {
  int sum = 0;
  const int n = completed_stage_count < 0
                    ? 0
                    : (completed_stage_count > k_stage_count
                           ? k_stage_count
                           : completed_stage_count);
  for (int i = 0; i < n; ++i) {
    sum += k_stage_weights[i];
  }
  return sum;
}

const char* engineOpenProgressTitle() { return "Opening editor"; }

const char* engineOpenProgressStageCaption(EngineOpenProgressStage stage) {
  switch (stage) {
    case EngineOpenProgressStage::openingScene:
      return "Opening scene";
    case EngineOpenProgressStage::indexingContent:
      return "Indexing content";
    default:
      return "Opening scene";
  }
}

const char* engineOpenProgressCaption() {
  if (g_completed_stages <= k_cover_completed_stages) {
    return engineOpenProgressStageCaption(
        EngineOpenProgressStage::openingScene);
  }
  return engineOpenProgressStageCaption(
      EngineOpenProgressStage::indexingContent);
}

void engineOpenProgressResetForTest() {
  g_visible = false;
  g_consumed = false;
  g_session_ended_by_close = false;
  g_session_start_set = false;
  g_completed_stages = 0;
  g_last_presented_elapsed = -1;
  unbindHost();
}

void engineOpenProgressNoteSessionStart(
    std::chrono::steady_clock::time_point start) {
  g_session_start = start;
  g_session_start_set = true;
}

int engineOpenProgressElapsedSeconds(std::chrono::steady_clock::time_point now) {
  if (!g_session_start_set) {
    return 0;
  }
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - g_session_start);
  if (elapsed.count() < 0) {
    return 0;
  }
  return static_cast<int>(elapsed.count());
}

int engineOpenProgressElapsedSeconds() {
  return engineOpenProgressElapsedSeconds(std::chrono::steady_clock::now());
}

bool engineOpenProgressTryShow(EngineHostMode mode, bool headless,
                               bool project_manager, WindowSystem* window,
                               EngineOpenProgressPresent present) {
  if (g_consumed || g_visible) {
    return g_visible;
  }
  if (!engineOpenProgressShouldMount(mode, headless, project_manager)) {
    return false;
  }
  g_visible = true;
  g_session_ended_by_close = false;
  g_completed_stages = k_cover_completed_stages;
  g_window = window;
  g_present = eastl::move(present);
  setBootWorkHeartbeat([]() { return engineOpenProgressPump(); });
  presentNow();
  return true;
}

bool engineOpenProgressIsVisible() { return g_visible; }

bool engineOpenProgressIsConsumed() { return g_consumed; }

int engineOpenProgressPercent() {
  return engineOpenProgressCompletedPercent(g_completed_stages);
}

int engineOpenProgressCompletedStageCount() { return g_completed_stages; }

void engineOpenProgressCompleteOpeningScene() {
  if (!g_visible) {
    return;
  }
  g_completed_stages = 4;
  presentNow();
}

void engineOpenProgressCompleteIndexing() {
  if (!g_visible) {
    g_consumed = true;
    return;
  }
  g_completed_stages = 5;
  presentNow();
  engineOpenProgressDismiss();
}

void engineOpenProgressDismiss() {
  if (g_visible) {
    g_consumed = true;
  }
  hideAndUnbind();
}

void engineOpenProgressNotifyLaterOpenScene() {
  if (g_visible) {
    return;
  }
  g_consumed = true;
}

void engineOpenProgressNoteCloseRequested() {
  if (!g_visible) {
    return;
  }
  g_session_ended_by_close = true;
  g_consumed = true;
  hideAndUnbind();
}

bool engineOpenProgressPump() {
  if (!g_visible) {
    return true;
  }
  if (g_window) {
    g_window->pumpEvents();
    if (g_window->shouldClose()) {
      engineOpenProgressNoteCloseRequested();
      return false;
    }
  }
  const auto now = std::chrono::steady_clock::now();
  const int elapsed = engineOpenProgressElapsedSeconds(now);
  const bool second_ticked = elapsed != g_last_presented_elapsed;
  const bool due =
      g_last_present.time_since_epoch().count() == 0 ||
      (now - g_last_present) >= std::chrono::milliseconds(100);
  if (second_ticked || due) {
    presentNow();
  }
  return true;
}

bool engineOpenProgressOffersRetry() { return false; }

bool engineOpenProgressSessionEndedByClose() {
  return g_session_ended_by_close;
}

}  // namespace Blunder
