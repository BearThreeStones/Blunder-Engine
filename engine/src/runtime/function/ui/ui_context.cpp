#include "runtime/function/ui/ui_context.h"

#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/content_browser/content_browser_system.h"

namespace Blunder {

void UiContext::bindServices(const EditorServiceHandles& handles) {
  m_handles = handles;
}

void UiContext::beginShutdown() {
  m_state.store(State::draining, std::memory_order_release);
  ++m_generation;
  m_handles = {};
}

std::optional<UiContext::LockedServices> UiContext::lockEditorServices() const {
  if (!isRunning()) {
    return std::nullopt;
  }

  LockedServices locked{};
  if (auto selection = m_handles.selection.lock()) {
    locked.selection = eastl::move(selection);
  }
  if (auto hierarchy = m_handles.hierarchy.lock()) {
    locked.hierarchy = eastl::move(hierarchy);
  }
  if (auto scene = m_handles.scene.lock()) {
    locked.scene = eastl::move(scene);
  }
  if (auto content_browser = m_handles.content_browser.lock()) {
    locked.content_browser = eastl::move(content_browser);
  }
  if (auto editor_scene_edit = m_handles.editor_scene_edit.lock()) {
    locked.editor_scene_edit = eastl::move(editor_scene_edit);
  }
  if (auto render_system = m_handles.render_system.lock()) {
    locked.render_system = eastl::move(render_system);
  }
  if (auto asset_compiler = m_handles.asset_compiler.lock()) {
    locked.asset_compiler = eastl::move(asset_compiler);
  }
  if (auto asset_import = m_handles.asset_import.lock()) {
    locked.asset_import = eastl::move(asset_import);
  }

  return locked;
}

}  // namespace Blunder
