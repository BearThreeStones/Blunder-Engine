#pragma once

#include "EASTL/functional.h"

namespace Blunder {

using BootWorkHeartbeat = eastl::function<bool()>;

void setBootWorkHeartbeat(BootWorkHeartbeat heartbeat);

/// True means keep working. An unset heartbeat always continues.
bool bootWorkHeartbeatContinue();

}  // namespace Blunder
