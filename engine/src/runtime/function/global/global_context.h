#pragma once

#include "EASTL/memory.h"
#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"

#include <filesystem>

#include "runtime/core/memory/memory_system.h"
#include "runtime/function/global/engine_host_mode.h"

namespace Blunder {
class LogSystem;
class InputSystem;
class SlintSystem;
class UiHost;

class IViewportSink;
class UIViewportBridge;
// class PhysicsManager;
class FileSystem;
class AssetManager;
class AssetRegistry;
class AssetImportService;
class AssetCompilerService;
class ThumbnailGenerator;
class ContentBrowserSystem;
class EditorSelectionSystem;
class HierarchySystem;
class EditorSceneEditSystem;
class DocumentHistory;
class ViewportPickSystem;
// class ConfigManager;
// class WorldManager;
class RenderSystem;
class SceneSystem;
class WindowSystem;
class LayerStack;
class DotNetHost;
class PlaySessionController;
// class ParticleManager;

struct EngineInitParams;

/// Manage the lifetime and creation/destruction order of all global system
class RuntimeGlobalContext {
 public:
  // create all global systems and initialize these systems
  void startSystems(
      const std::filesystem::path& project_root = std::filesystem::path{},
      EngineHostMode host_mode = EngineHostMode::Editor,
      const eastl::string& play_scene = {});
  // destroy all global systems
  void shutdownSystems();

  EngineHostMode hostMode() const { return m_host_mode; }

  /// Player Pause: skip Behaviour Tick while keeping render/orbit alive.
  bool isPlayPaused() const { return m_play_paused; }
  void setPlayPaused(bool paused) { m_play_paused = paused; }

 public:
  MemorySystem m_memory_system;
  eastl::shared_ptr<LogSystem> m_logger_system;
  eastl::shared_ptr<InputSystem> m_input_system;
  eastl::shared_ptr<FileSystem> m_file_system;
  eastl::shared_ptr<AssetRegistry> m_asset_registry;
  eastl::shared_ptr<AssetManager> m_asset_manager;
  eastl::shared_ptr<AssetImportService> m_asset_import;
  eastl::shared_ptr<AssetCompilerService> m_asset_compiler;
  eastl::shared_ptr<SceneSystem> m_scene_system;
  eastl::shared_ptr<ThumbnailGenerator> m_thumbnail_generator;
  eastl::shared_ptr<ContentBrowserSystem> m_content_browser;
  eastl::shared_ptr<EditorSelectionSystem> m_editor_selection;
  eastl::shared_ptr<HierarchySystem> m_hierarchy;
  eastl::shared_ptr<EditorSceneEditSystem> m_editor_scene_edit;
  eastl::shared_ptr<DocumentHistory> m_document_history;
  eastl::shared_ptr<ViewportPickSystem> m_viewport_pick;
  // eastl::shared_ptr<ConfigManager> m_config_manager;
  // eastl::shared_ptr<WorldManager> m_world_manager;
  // eastl::shared_ptr<PhysicsManager> m_physics_manager;
  eastl::shared_ptr<WindowSystem> m_window_system;
  eastl::shared_ptr<RenderSystem> m_render_system;
  eastl::shared_ptr<UiHost> m_ui_host;
  eastl::shared_ptr<SlintSystem> m_slint_system;
  eastl::unique_ptr<IViewportSink> m_viewport_sink;
  eastl::unique_ptr<UIViewportBridge> m_viewport_bridge;
  eastl::shared_ptr<LayerStack> m_layer_stack;
  /// In-process CoreCLR host. Player always starts it for the Play session;
  /// Edit Mode starts only with BLUNDER_DOTNET_SCRIPTS=1 (debug/tests — not product
  /// Play). Null or !isRunning() is normal in the editor. See docs/agents/testing.md.
  eastl::unique_ptr<DotNetHost> m_dotnet_host;
  /// Editor Play session (spawn engine_player + IPC). Null in Player host mode.
  eastl::unique_ptr<PlaySessionController> m_play_session;
  // eastl::shared_ptr<ParticleManager> m_particle_manager;

 private:
  EngineHostMode m_host_mode{EngineHostMode::Editor};
  bool m_play_paused{false};
};

extern RuntimeGlobalContext g_runtime_global_context;
}  // namespace Blunder
