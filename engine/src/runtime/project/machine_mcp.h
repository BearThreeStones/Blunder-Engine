#pragma once

#include "runtime/project/editor_launch.h"
#include "runtime/project/machine_adapter.h"

#include <string>

namespace Blunder {

bool mcpStdinHasBytes();
bool mcpReadMessage(std::string& json);
void mcpWriteMessage(const std::string& json);
std::string mcpHandleMessage(const std::string& request,
                             const EditorSessionLaunch& session,
                             MachineAdapterHost& host);

}  // namespace Blunder
