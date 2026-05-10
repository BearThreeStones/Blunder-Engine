#include "runtime/core/log/log_access.h"

#include "runtime/function/global/global_context.h"

namespace Blunder {

LogSystem* getLogSystem() {
  return g_runtime_global_context.m_logger_system.get();
}

}  // namespace Blunder
