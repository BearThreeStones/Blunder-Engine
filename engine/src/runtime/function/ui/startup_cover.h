#pragma once

#include <cstdint>

#include "EASTL/string.h"

#include "runtime/function/global/engine_host_mode.h"

namespace Blunder {

class WindowSystem;

enum class StartupCoverPhase : uint8_t {
  cookingAssets = 0,
  preparingEditor,
  startingEditor,
};

bool startupCoverShouldMount(EngineHostMode mode, bool headless);
const char* startupCoverStageName(StartupCoverPhase phase);

void startupCoverBegin(WindowSystem* window, const eastl::string& wordmark);
void startupCoverSetPhase(StartupCoverPhase phase);
/// Pump close/resize/expose and repaint. False if the user closed the window.
bool startupCoverPump();
void startupCoverDismiss();
bool startupCoverIsActive();

}  // namespace Blunder
