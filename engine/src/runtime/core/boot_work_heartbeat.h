#pragma once

#include "EASTL/functional.h"

namespace Blunder {

using BootWorkHeartbeat = eastl::function<bool()>;

void setBootWorkHeartbeat(BootWorkHeartbeat heartbeat);

/// True means keep working. An unset heartbeat always continues. After overlay
/// close the callback stays installed and returns false until boot unwinds.
bool bootWorkHeartbeatContinue();

}  // namespace Blunder
