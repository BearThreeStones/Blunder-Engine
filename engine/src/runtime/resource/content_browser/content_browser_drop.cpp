#include "runtime/resource/content_browser/content_browser_drop.h"

#include <cstring>

namespace Blunder {

namespace {

bool endsWithSuffix(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
         0;
}

}  // namespace

ContentBrowserDropKind classifyContentBrowserDrop(
    const eastl::string& virtual_path) {
  if (endsWithSuffix(virtual_path, ".mesh.yaml") ||
      endsWithSuffix(virtual_path, ".mesh.asset")) {
    return ContentBrowserDropKind::mesh;
  }
  if (endsWithSuffix(virtual_path, ".scene.asset")) {
    return ContentBrowserDropKind::scene;
  }
  return ContentBrowserDropKind::other;
}

ContentBrowserDragCursorKind resolveContentBrowserDragCursor(
    bool is_dragging, bool over_viewport, bool over_folder,
    ContentBrowserDropKind kind) {
  if (!is_dragging) {
    return ContentBrowserDragCursorKind::default_arrow;
  }
  if (over_viewport && (kind == ContentBrowserDropKind::mesh ||
                        kind == ContentBrowserDropKind::scene)) {
    return ContentBrowserDragCursorKind::pointer;
  }
  if (over_folder) {
    return ContentBrowserDragCursorKind::move;
  }
  return ContentBrowserDragCursorKind::not_allowed;
}

}  // namespace Blunder
