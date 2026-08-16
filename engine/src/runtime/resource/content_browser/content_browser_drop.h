#pragma once

#include "EASTL/string.h"

namespace Blunder {

enum class ContentBrowserDropKind { other, mesh, scene };

enum class ContentBrowserDragCursorKind {
  default_arrow,
  pointer,
  move,
  not_allowed
};

ContentBrowserDropKind classifyContentBrowserDrop(
    const eastl::string& virtual_path);

/// Three-state cursor: pointer (viewport mesh/scene), move (folder), not-allowed.
ContentBrowserDragCursorKind resolveContentBrowserDragCursor(
    bool is_dragging, bool over_viewport, bool over_folder,
    ContentBrowserDropKind kind);

}  // namespace Blunder
