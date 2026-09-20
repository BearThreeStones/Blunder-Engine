#include "runtime/core/boot_work_heartbeat.h"

namespace Blunder {
namespace {

BootWorkHeartbeat g_heartbeat;

}  // namespace

void setBootWorkHeartbeat(BootWorkHeartbeat heartbeat) {
  g_heartbeat = eastl::move(heartbeat);
}

bool bootWorkHeartbeatContinue() {
  if (!g_heartbeat) {
    return true;
  }
  return g_heartbeat();
}

}  // namespace Blunder
