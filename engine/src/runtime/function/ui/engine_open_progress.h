#pragma once

#include <chrono>
#include <cstdint>

#include "EASTL/functional.h"

#include "runtime/function/global/engine_host_mode.h"

namespace Blunder {

class WindowSystem;

enum class EngineOpenProgressStage : uint8_t {
  cookingAssets = 0,
  preparingEditor,
  startingEditor,
  openingScene,
  indexingContent,
};

using EngineOpenProgressPresent = eastl::function<void()>;

bool engineOpenProgressShouldMount(EngineHostMode mode, bool headless,
                                   bool project_manager);
int engineOpenProgressStageWeight(EngineOpenProgressStage stage);
int engineOpenProgressCompletedPercent(int completed_stage_count);
const char* engineOpenProgressTitle();
const char* engineOpenProgressCaption();
const char* engineOpenProgressStageCaption(EngineOpenProgressStage stage);

void engineOpenProgressResetForTest();
void engineOpenProgressNoteSessionStart(
    std::chrono::steady_clock::time_point start);
int engineOpenProgressElapsedSeconds(std::chrono::steady_clock::time_point now);
int engineOpenProgressElapsedSeconds();

bool engineOpenProgressTryShow(EngineHostMode mode, bool headless,
                               bool project_manager, WindowSystem* window,
                               EngineOpenProgressPresent present);
bool engineOpenProgressIsVisible();
bool engineOpenProgressIsConsumed();
int engineOpenProgressPercent();
int engineOpenProgressCompletedStageCount();

void engineOpenProgressCompleteOpeningScene();
void engineOpenProgressCompleteIndexing();
void engineOpenProgressDismiss();
void engineOpenProgressNotifyLaterOpenScene();
void engineOpenProgressNoteCloseRequested();
bool engineOpenProgressPump();
bool engineOpenProgressOffersRetry();
bool engineOpenProgressSessionEndedByClose();

}  // namespace Blunder
