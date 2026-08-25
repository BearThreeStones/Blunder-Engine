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
class MeshPreviewOffscreenBackend;
class MeshPreviewRenderService;
class SceneThumbnailRenderService;
class ContentBrowserSystem;
class EditorSelectionSystem;
class HierarchySystem;
class EditorSceneEditSystem;
class DocumentHistory;
class ViewportPickSystem;
class PlacementPreviewController;
// class ConfigManager;
// class WorldManager;
class RenderSystem;
class SceneSystem;
class WindowSystem;
class LayerStack;
class DotNetHost;
class PlaySessionController;
class AnimationPreviewController;
class AnimationSyncCinePreviewController;
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

  bool contentBrowserHasInputFocus() const {
    return m_content_browser_input_focus;
  }
  void setContentBrowserHasInputFocus(bool focused) {
    m_content_browser_input_focus = focused;
    if (focused) {
      m_inspector_input_focus = false;
      m_attachment_preview_input_focus = false;
    }
  }
  bool inlineRenameActive() const { return m_inline_rename_active; }
  void setInlineRenameActive(bool active) { m_inline_rename_active = active; }

  bool inspectorHasInputFocus() const { return m_inspector_input_focus; }
  void setInspectorHasInputFocus(bool focused) {
    m_inspector_input_focus = focused;
  }
  bool inspectorAssetMode() const { return m_inspector_asset_mode; }
  void setInspectorAssetMode(bool asset_mode) {
    m_inspector_asset_mode = asset_mode;
  }
  bool assetInspectorHasUndoFocus() const {
    return m_inspector_input_focus && m_inspector_asset_mode;
  }
  bool attachmentPreviewHasInputFocus() const {
    return m_attachment_preview_input_focus;
  }
  void setAttachmentPreviewHasInputFocus(bool focused) {
    m_attachment_preview_input_focus = focused;
    if (focused) {
      m_content_browser_input_focus = false;
    }
  }

  /// Closes every Attachment property preview card (document swap / openScene).
  void closeAttachmentPreviewCards();

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
  eastl::unique_ptr<MeshPreviewOffscreenBackend> m_mesh_preview_backend;
  eastl::unique_ptr<MeshPreviewRenderService> m_mesh_preview_service;
  eastl::unique_ptr<SceneThumbnailRenderService> m_scene_thumbnail_service;
  eastl::shared_ptr<ContentBrowserSystem> m_content_browser;
  eastl::shared_ptr<EditorSelectionSystem> m_editor_selection;
  eastl::shared_ptr<HierarchySystem> m_hierarchy;
  eastl::shared_ptr<EditorSceneEditSystem> m_editor_scene_edit;
  eastl::unique_ptr<PlacementPreviewController> m_placement_preview;
  eastl::shared_ptr<DocumentHistory> m_document_history;
  eastl::shared_ptr<DocumentHistory> m_global_history;
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
  /// Edit Mode AnimationPlayer viewport preview (no DotNetHost).
  eastl::unique_ptr<AnimationPreviewController> m_animation_preview;
  /// Edit Mode Sync Group + CINE preview (no DotNetHost / Behaviour Tick).
  eastl::unique_ptr<AnimationSyncCinePreviewController> m_animation_sync_cine_preview;
  // eastl::shared_ptr<ParticleManager> m_particle_manager;

 private:
  EngineHostMode m_host_mode{EngineHostMode::Editor};
  bool m_play_paused{false};
  bool m_content_browser_input_focus{false};
  bool m_inline_rename_active{false};
  bool m_inspector_input_focus{false};
  bool m_inspector_asset_mode{false};
  bool m_attachment_preview_input_focus{false};
};

extern RuntimeGlobalContext g_runtime_global_context;
}  // namespace Blunder
