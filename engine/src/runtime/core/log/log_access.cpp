#include "runtime/core/log/log_access.h"

#include "runtime/core/log/console_ring.h"
#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/global/global_context.h"

namespace Blunder {

LogSystem* getLogSystem() {
  return g_runtime_global_context.m_logger_system.get();
}

ConsoleOrigin consoleOriginForHost() {
  if (g_runtime_global_context.hostMode() == EngineHostMode::Player) {
    return ConsoleOrigin::PlayProcess;
  }
  return ConsoleOrigin::EditorSession;
}

}  // namespace Blunder
