#include "runtime/function/global/global_context.h"

#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/log/log_system.h"
#include "runtime/engine.h"
// #include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/render/presenter/cpu_rgba8_viewport_presenter.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/platform/input/input_system.h"
// #include "runtime/function/particle/particle_manager.h"
// #include "runtime/function/physics/physics_manager.h"
// #include "runtime/function/render/debugdraw/debug_draw_manager.h"
// #include "runtime/function/render/render_debug_config.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_cook/asset_compiler_service.h"
#include "runtime/resource/thumbnail/thumbnail_generator.h"
#include "runtime/resource/content_browser/content_browser_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
// #include "runtime/resource/config_manager/config_manager.h"

namespace Blunder {
RuntimeGlobalContext g_runtime_global_context;

void RuntimeGlobalContext::startSystems() {
  m_memory_system.initialize();

  // m_config_manager = eastl::make_shared<ConfigManager>();
  // m_config_manager->initialize(config_file_path);

  m_logger_system = eastl::make_shared<LogSystem>();

  // FileSystem must be live before any system that touches disk (asset
  // loading, shader compilation, configs, ...).
  m_file_system = eastl::make_shared<FileSystem>();
  m_file_system->initialize();

  m_asset_registry = eastl::make_shared<AssetRegistry>();
  m_asset_registry->initialize(m_file_system.get());

  m_asset_manager = eastl::make_shared<AssetManager>();
  AssetManagerInitInfo asset_init_info;
  asset_init_info.file_system = m_file_system.get();
  m_asset_manager->initialize(asset_init_info);

  m_asset_compiler = eastl::make_shared<AssetCompilerService>();
  m_asset_compiler->initialize(m_file_system.get(), m_asset_manager.get(),
                                 m_asset_registry.get());
  m_asset_compiler->cookIfStale();

  m_asset_import = eastl::make_shared<AssetImportService>();

  m_scene_system = eastl::make_shared<SceneSystem>();
  SceneSystemInitInfo scene_init_info{};
  scene_init_info.asset_manager = m_asset_manager.get();
  m_scene_system->initialize(scene_init_info);

  m_editor_selection = eastl::make_shared<EditorSelectionSystem>();
  m_hierarchy = eastl::make_shared<HierarchySystem>();
  m_editor_scene_edit = eastl::make_shared<EditorSceneEditSystem>();
  m_editor_scene_edit->initialize(m_file_system.get(), m_asset_manager.get(),
                                    m_scene_system.get());

  m_thumbnail_generator = eastl::make_shared<ThumbnailGenerator>();
  ThumbnailGeneratorInit thumbnail_init{};
  thumbnail_init.file_system = m_file_system.get();
  thumbnail_init.asset_manager = m_asset_manager.get();
  m_thumbnail_generator->initialize(thumbnail_init);

  m_content_browser = eastl::make_shared<ContentBrowserSystem>();
  ContentBrowserInit browser_init{};
  browser_init.file_system = m_file_system.get();
  browser_init.asset_manager = m_asset_manager.get();
  browser_init.thumbnail_generator = m_thumbnail_generator.get();
  m_content_browser->initialize(browser_init);
  AssetImportServiceInit import_init{};
  import_init.file_system = m_file_system.get();
  import_init.asset_registry = m_asset_registry.get();
  import_init.content_browser = m_content_browser.get();
  m_asset_import->initialize(import_init);
  m_content_browser->startFileWatch();

  // m_physics_manager = eastl::make_shared<PhysicsManager>();
  // m_physics_manager->initialize();

  // m_world_manager = eastl::make_shared<WorldManager>();
  // m_world_manager->initialize();

  m_window_system = eastl::make_shared<WindowSystem>();
  WindowCreateInfo window_create_info;
  m_window_system->initialize(window_create_info);

  // Slint UI (D3D12 on Windows) before the engine's headless Vulkan device.
  m_slint_system = eastl::make_shared<SlintSystem>();
  SlintSystemInitInfo slint_init_info;
  slint_init_info.window_system = m_window_system.get();
  m_slint_system->initialize(slint_init_info);

  m_viewport_presenter =
      eastl::make_unique<presenter::CpuRgba8ViewportPresenter>(m_slint_system.get());

  m_render_system = eastl::make_shared<RenderSystem>();
  RenderSystemInitInfo render_init_info;
  render_init_info.asset_manager = m_asset_manager.get();
  render_init_info.window_system = m_window_system.get();
  render_init_info.viewport_presenter = m_viewport_presenter.get();
  render_init_info.viewport_layout_source = m_slint_system.get();
#ifdef NDEBUG
  render_init_info.enable_validation = false;
#else
  render_init_info.enable_validation = true;
#endif
  m_render_system->initialize(render_init_info);

  m_input_system = eastl::make_shared<InputSystem>();
  m_input_system->initialize();

  m_layer_stack = eastl::make_shared<LayerStack>();

  // m_particle_manager = eastl::make_shared<ParticleManager>();
  // m_particle_manager->initialize();

  // m_debugdraw_manager = eastl::make_shared<DebugDrawManager>();
  // m_debugdraw_manager->initialize();

  // m_render_debug_config = eastl::make_shared<RenderDebugConfig>();
}

void RuntimeGlobalContext::shutdownSystems() {
  // m_render_debug_config.reset();

  // m_debugdraw_manager.reset();

  m_layer_stack.reset();

  m_viewport_presenter.reset();

  if (m_slint_system) {
    m_slint_system->shutdown();
    m_slint_system.reset();
  }

  if (m_render_system) {
    m_render_system->shutdown();
    m_render_system.reset();
  }

  if (m_input_system) {
    m_input_system->shutdown();
    m_input_system.reset();
  }

  m_window_system.reset();

  // m_world_manager->clear();
  // m_world_manager.reset();

  // m_physics_manager->clear();
  // m_physics_manager.reset();

  if (m_content_browser) {
    m_content_browser->shutdown();
    m_content_browser.reset();
  }

  if (m_asset_import) {
    m_asset_import->shutdown();
    m_asset_import.reset();
  }

  if (m_asset_compiler) {
    m_asset_compiler->shutdown();
    m_asset_compiler.reset();
  }

  if (m_thumbnail_generator) {
    m_thumbnail_generator->shutdown();
    m_thumbnail_generator.reset();
  }

  m_editor_scene_edit.reset();
  m_hierarchy.reset();
  m_editor_selection.reset();

  if (m_scene_system) {
    m_scene_system->shutdown();
    m_scene_system.reset();
  }

  if (m_asset_manager) {
    m_asset_manager->shutdown();
    m_asset_manager.reset();
  }

  if (m_asset_registry) {
    m_asset_registry->shutdown();
    m_asset_registry.reset();
  }

  if (m_file_system) {
    m_file_system->shutdown();
    m_file_system.reset();
  }

  m_logger_system.reset();

  m_memory_system.shutdown();

  // m_config_manager.reset();

  // m_particle_manager.reset();
}
}  // namespace Blunder
