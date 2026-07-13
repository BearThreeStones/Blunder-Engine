#pragma once

#include "EASTL/unique_ptr.h"

#include "runtime/function/editor/document_history.h"

namespace Blunder {

/// Push an already-applied command and mark the active scene dirty.
void pushDocumentCommand(eastl::unique_ptr<IEditorCommand> command);

SelectionSnapshot currentSelectionSnapshot();

}  // namespace Blunder
