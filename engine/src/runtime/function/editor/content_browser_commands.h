#pragma once

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"

#include "runtime/function/editor/document_history.h"

namespace Blunder {

class ContentBrowserSystem;

eastl::unique_ptr<IEditorCommand> makeCreateFolderCommand(
    ContentBrowserSystem* browser, eastl::string virtual_path);

eastl::unique_ptr<IEditorCommand> makeRenameEntryCommand(
    ContentBrowserSystem* browser, eastl::string from_path,
    eastl::string to_path, eastl::string from_name, eastl::string to_name,
    bool is_directory);

eastl::unique_ptr<IEditorCommand> makeReparentEntryCommand(
    ContentBrowserSystem* browser, eastl::string from_path,
    eastl::string dest_parent, eastl::string original_parent,
    bool is_directory);

}  // namespace Blunder
