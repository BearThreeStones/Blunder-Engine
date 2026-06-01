#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

#include "EASTL/shared_ptr.h"

#include "runtime/function/ui/editor_service_handles.h"

namespace Blunder {

class EditorSelectionSystem;
class HierarchySystem;
class SceneSystem;
class ContentBrowserSystem;
class EditorSceneEditSystem;
class RenderSystem;
class AssetCompilerService;
class AssetImportService;

class UiContext final {
 public:
  enum class State : uint8_t { running, draining };

  void bindServices(const EditorServiceHandles& handles);
  void beginShutdown();

  State state() const { return m_state.load(std::memory_order_acquire); }
  bool isRunning() const { return state() == State::running; }
  uint64_t generation() const { return m_generation.load(std::memory_order_acquire); }

  struct LockedServices {
    eastl::shared_ptr<EditorSelectionSystem> selection;
    eastl::shared_ptr<HierarchySystem> hierarchy;
    eastl::shared_ptr<SceneSystem> scene;
    eastl::shared_ptr<ContentBrowserSystem> content_browser;
    eastl::shared_ptr<EditorSceneEditSystem> editor_scene_edit;
    eastl::shared_ptr<RenderSystem> render_system;
    eastl::shared_ptr<AssetCompilerService> asset_compiler;
    eastl::shared_ptr<AssetImportService> asset_import;
  };

  std::optional<LockedServices> lockEditorServices() const;

 private:
  std::atomic<State> m_state{State::running};
  std::atomic<uint64_t> m_generation{0};
  EditorServiceHandles m_handles;
};

}  // namespace Blunder
