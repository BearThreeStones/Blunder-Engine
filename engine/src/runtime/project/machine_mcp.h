#pragma once

#include "runtime/project/editor_launch.h"
#include "runtime/project/machine_adapter.h"

#include <string>

namespace Blunder {

bool mcpStdinHasBytes();
bool mcpStdinClosed();
bool mcpReadMessage(std::string& json);
void mcpWriteMessage(const std::string& json);
/// Duplicate stdin/stdout before SDL_Log AttachConsole replaces them.
void mcpCaptureStdioHandles();
/// Handshake methods (`initialize`, `initialized`, `tools/list`, `ping`) do
/// not need Slang, GPU, or a loaded `--project-root`. Only `tools/call` does.
bool mcpMessageNeedsEngine(const std::string& request);
std::string mcpHandleMessage(const std::string& request,
                             const EditorSessionLaunch& session,
                             MachineAdapterHost& host);

}  // namespace Blunder
