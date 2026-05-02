#include "runtime/function/global/global_context.h"

#include "runtime/core/layer/layer_stack.h"
#include "runtime/core/log/log_system.h"
#include "runtime/engine.h"
// #include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/vulkan/vulkan_swapchain.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/platform/input/input_system.h"
// #include "runtime/function/particle/particle_manager.h"
// #include "runtime/function/physics/physics_manager.h"
// #include "runtime/function/render/debugdraw/debug_draw_manager.h"
// #include "runtime/function/render/render_debug_config.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"
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

  m_asset_manager = eastl::make_shared<AssetManager>();
  AssetManagerInitInfo asset_init_info;
  asset_init_info.file_system = m_file_system.get();
  m_asset_manager->initialize(asset_init_info);

  // m_physics_manager = eastl::make_shared<PhysicsManager>();
  // m_physics_manager->initialize();

  // m_world_manager = eastl::make_shared<WorldManager>();
  // m_world_manager->initialize();

  m_window_system = eastl::make_shared<WindowSystem>();
  WindowCreateInfo window_create_info;
  m_window_system->initialize(window_create_info);

  // RenderSystem must be created before UI systems (owns Vulkan context)
  m_render_system = eastl::make_shared<RenderSystem>();
  RenderSystemInitInfo render_init_info;
  render_init_info.window_system = m_window_system.get();
#ifdef NDEBUG
  render_init_info.enable_validation = false;
#else
  render_init_info.enable_validation = true;
#endif
  m_render_system->initialize(render_init_info);

  m_slint_system = eastl::make_shared<SlintSystem>();
  SlintSystemInitInfo slint_init_info;
  slint_init_info.window_system = m_window_system.get();
  m_slint_system->initialize(slint_init_info);

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

  if (m_asset_manager) {
    m_asset_manager->shutdown();
    m_asset_manager.reset();
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
