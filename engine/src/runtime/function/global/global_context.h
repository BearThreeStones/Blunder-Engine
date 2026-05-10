#pragma once

#include "EASTL/memory.h"
#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "runtime/core/memory/memory_system.h"

namespace Blunder {
class LogSystem;
class InputSystem;
class SlintSystem;
// class PhysicsManager;
class FileSystem;
class AssetManager;
// class ConfigManager;
// class WorldManager;
class RenderSystem;
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
  eastl::shared_ptr<AssetManager> m_asset_manager;
  // eastl::shared_ptr<ConfigManager> m_config_manager;
  // eastl::shared_ptr<WorldManager> m_world_manager;
  // eastl::shared_ptr<PhysicsManager> m_physics_manager;
  eastl::shared_ptr<WindowSystem> m_window_system;
  eastl::shared_ptr<RenderSystem> m_render_system;
  eastl::shared_ptr<SlintSystem> m_slint_system;
  eastl::shared_ptr<LayerStack> m_layer_stack;
  // eastl::shared_ptr<ParticleManager> m_particle_manager;
  // eastl::shared_ptr<DebugDrawManager> m_debugdraw_manager;
  // eastl::shared_ptr<RenderDebugConfig> m_render_debug_config;
};

extern RuntimeGlobalContext g_runtime_global_context;
}  // namespace Blunder
