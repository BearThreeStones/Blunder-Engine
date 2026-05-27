#pragma once

#include "EASTL/memory.h"
#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "runtime/core/memory/memory_system.h"

namespace Blunder {
class LogSystem;
class InputSystem;
class SlintSystem;

namespace presenter {
class CpuRgba8ViewportPresenter;
}
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
// class ConfigManager;
// class WorldManager;
class RenderSystem;
class SceneSystem;
class WindowSystem;
class LayerStack;
// class ParticleManager;
// class DebugDrawManager;
// class RenderDebugConfig;

struct EngineInitParams;

/// Manage the lifetime and creation/destruction order of all global system
class RuntimeGlobalContext {
 public:
  // create all global systems and initialize these systems
  void startSystems();
  // destroy all global systems
  void shutdownSystems();

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
  // eastl::shared_ptr<ConfigManager> m_config_manager;
  // eastl::shared_ptr<WorldManager> m_world_manager;
  // eastl::shared_ptr<PhysicsManager> m_physics_manager;
  eastl::shared_ptr<WindowSystem> m_window_system;
  eastl::shared_ptr<RenderSystem> m_render_system;
  eastl::shared_ptr<SlintSystem> m_slint_system;
  eastl::unique_ptr<presenter::CpuRgba8ViewportPresenter> m_viewport_presenter;
  eastl::shared_ptr<LayerStack> m_layer_stack;
  // eastl::shared_ptr<ParticleManager> m_particle_manager;
  // eastl::shared_ptr<DebugDrawManager> m_debugdraw_manager;
  // eastl::shared_ptr<RenderDebugConfig> m_render_debug_config;
};

extern RuntimeGlobalContext g_runtime_global_context;
}  // namespace Blunder
