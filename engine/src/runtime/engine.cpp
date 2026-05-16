#include "runtime/engine.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/event/application_event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/core/layer/layer.h"
#include "runtime/core/layer/layer_stack.h"
// #include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/input/input_system.h"
// #include "runtime/function/particle/particle_manager.h"
// #include "runtime/function/physics/physics_manager.h"
// #include "runtime/function/render/debugdraw/debug_draw_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {
bool g_is_editor_mode{false};
eastl::unordered_set<eastl::string> g_editor_tick_component_types{};

namespace {

constexpr const char* k_startup_demo_mesh_path =
  "models/textured_cube/textured_cube.gltf";

}  // namespace

void BlunderEngine::startEngine() {
  g_runtime_global_context.startSystems();

  // Register event callback for testing
  g_runtime_global_context.m_window_system->setEventCallback(
      [](Event& e) { onEvent(e); });

  LOG_INFO("engine start");
}

void BlunderEngine::onEvent(Event& e) {
  // Propagate to layers in reverse order (overlays first)
  // This allows overlays to consume events before game layers
  auto& layer_stack = g_runtime_global_context.m_layer_stack;
  for (auto it = layer_stack->rbegin(); it != layer_stack->rend(); ++it) {
    if (e.handled) break;
    (*it)->onEvent(e);
  }

  if (!e.handled && g_runtime_global_context.m_render_system) {
    g_runtime_global_context.m_render_system->onEvent(e);
  }

  // Log events for debugging (skip high-frequency or noisy types)
  if (e.getEventType() != EventType::MouseMoved &&
      e.getEventType() != EventType::KeyTyped && !e.handled) {
    LOG_DEBUG("[Event] {}", e.toString().c_str());
  }

  // Engine-level event handling
  EventDispatcher dispatcher(e);

  dispatcher.dispatch<WindowCloseEvent>([](WindowCloseEvent& event) {
    LOG_INFO("[Event] Window close requested");
    return true;
  });

  dispatcher.dispatch<WindowResizeEvent>([](WindowResizeEvent& event) {
    LOG_INFO("[Event] Window resized to {}x{}", event.getWidth(),
             event.getHeight());
    return true;
  });

  dispatcher.dispatch<KeyPressedEvent>([](KeyPressedEvent& event) {
    if (!event.isRepeat()) {
      LOG_DEBUG(
          "[Event] Key pressed: key={} scancode={} ctrl={} shift={} alt={}",
          event.getKeyCode(), event.getScanCode(), event.isCtrlDown(),
          event.isShiftDown(), event.isAltDown());
    }
    return false;
  });

  dispatcher.dispatch<KeyReleasedEvent>([](KeyReleasedEvent& event) {
    LOG_DEBUG("[Event] Key released: key={} scancode={}", event.getKeyCode(),
              event.getScanCode());
    return false;
  });

  dispatcher.dispatch<KeyTypedEvent>([](KeyTypedEvent& event) {
    LOG_DEBUG("[Event] Text input (UTF-8): {}", event.getUtf8().c_str());
    return false;
  });

  dispatcher.dispatch<MouseMovedEvent>([](MouseMovedEvent& event) {
    // LOG_DEBUG("[Event] Mouse moved: x={}, y={}", event.getX(), event.getY());
    return false;
  });
}

void BlunderEngine::shutdownEngine() {
  LOG_INFO("engine shutdown");

  g_runtime_global_context.shutdownSystems();
}

void BlunderEngine::initialize() {
  if (!g_runtime_global_context.m_asset_manager ||
      !g_runtime_global_context.m_render_system) {
    LOG_WARN("[BlunderEngine] initialize skipped startup asset verification because required systems are unavailable");
    return;
  }

  const eastl::shared_ptr<MeshAsset> demo_mesh =
      g_runtime_global_context.m_asset_manager->loadMesh(k_startup_demo_mesh_path);
  if (!demo_mesh) {
    LOG_ERROR("[BlunderEngine] failed to load startup demo mesh {}",
              k_startup_demo_mesh_path);
    return;
  }

  LOG_INFO(
      "[BlunderEngine] startup demo mesh ready {} (vertices={}, indices={}, stride={})",
      k_startup_demo_mesh_path, demo_mesh->getVertexCount(),
      demo_mesh->getIndexCount(), demo_mesh->getVertexStride());

  const eastl::shared_ptr<MaterialAsset>& material = demo_mesh->getMaterialAsset();
  if (!material || material->getBaseColorTextureAsset() == nullptr) {
    LOG_INFO("[BlunderEngine] startup demo mesh has no baseColor texture dependency");
    return;
  }

  const eastl::shared_ptr<Texture2DAsset>& base_color_texture =
      material->getBaseColorTextureAsset();
  if (g_runtime_global_context.m_render_system->ensureTextureUploaded(
          base_color_texture.get()) == nullptr) {
    LOG_ERROR("[BlunderEngine] failed to upload startup demo baseColor texture {}",
              base_color_texture->getVirtualPath().c_str());
    return;
  }

  LOG_INFO(
      "[BlunderEngine] startup demo baseColor ready {} ({}x{}, factor=({}, {}, {}, {}))",
      base_color_texture->getVirtualPath().c_str(),
      base_color_texture->getWidth(), base_color_texture->getHeight(),
      material->getBaseColorFactor().x, material->getBaseColorFactor().y,
      material->getBaseColorFactor().z, material->getBaseColorFactor().w);
}
void BlunderEngine::clear() {}

void BlunderEngine::pushLayer(Layer* layer) {
  g_runtime_global_context.m_layer_stack->pushLayer(layer);
  layer->onAttach();
}

void BlunderEngine::pushOverlay(Layer* overlay) {
  g_runtime_global_context.m_layer_stack->pushOverlay(overlay);
  overlay->onAttach();
}

void BlunderEngine::run() {
  eastl::shared_ptr<WindowSystem> window_system =
      g_runtime_global_context.m_window_system;
  ASSERT(window_system);

  m_frame_timer.reset();

  while (!window_system->shouldClose()) {
    const float delta_time = calculateDeltaTime();
    tickOneFrame(delta_time);
  }
}

float BlunderEngine::calculateDeltaTime() { return m_frame_timer.tick(); }

bool BlunderEngine::tickOneFrame(float delta_time) {
  g_runtime_global_context.m_memory_system.beginFrame();

  g_runtime_global_context.m_window_system->pumpEvents();
  g_runtime_global_context.m_input_system->tick();

  // 更新所有层（正向迭代：Layer1 → OverlayN）
  for (Layer* layer : *g_runtime_global_context.m_layer_stack) {
    layer->onUpdate(delta_time);
  }

  // logicalTick(delta_time);
  calculateFPS(delta_time);

  // single thread
  // exchange data between logic and render contexts

  // 1. Render 3D scene to off-screen RT, read back to CPU, push pixels into
  //    the Slint Image control via setViewportImage(). The target size comes
  //    from Slint's central viewport rectangle.
  rendererTick(delta_time);

  // 2. Slint composites the entire window (including the Image control with
  //    the freshly-pushed 3D viewport pixels) and presents to the HWND via
  //    its SkiaRenderer.
  g_runtime_global_context.m_slint_system->update();

#ifdef ENABLE_PHYSICS_DEBUG_RENDERER
  g_runtime_global_context.m_physics_manager->renderPhysicsWorld(delta_time);
#endif

  g_runtime_global_context.m_window_system->setTitle(
      std::string("Blunder - " + std::to_string(getFPS()) + " FPS").c_str());

  const bool should_window_close =
      g_runtime_global_context.m_window_system->shouldClose();
  return !should_window_close;
}

// void BlunderEngine::logicalTick(float delta_time) {
//   g_runtime_global_context.m_world_manager->tick(delta_time);
//   g_runtime_global_context.m_input_system->tick();
// }
//
bool BlunderEngine::rendererTick(float delta_time) {
  SlintSystem* slint_system = g_runtime_global_context.m_slint_system.get();
  static bool s_logged_viewport_ready = false;
  static uint32_t s_last_viewport_w = 0;
  static uint32_t s_last_viewport_h = 0;

  // Use the central viewport rectangle reported by Slint as the off-screen
  // RT target size, falling back to the drawable surface size before the
  // first Slint layout has computed it.
  uint32_t target_w = 0;
  uint32_t target_h = 0;
  if (slint_system) {
    eastl::array<uint32_t, 2> vp = slint_system->getViewportLogicalSize();
    target_w = vp[0];
    target_h = vp[1];
    if (target_w > 0 && target_h > 0) {
      if (!s_logged_viewport_ready) {
        LOG_INFO("[BlunderEngine] Slint viewport ready: {}x{}", target_w, target_h);
        s_logged_viewport_ready = true;
      } else if (target_w != s_last_viewport_w || target_h != s_last_viewport_h) {
        LOG_INFO("[BlunderEngine] Slint viewport changed: {}x{} -> {}x{}",
                 s_last_viewport_w, s_last_viewport_h, target_w, target_h);
      }
      s_last_viewport_w = target_w;
      s_last_viewport_h = target_h;
    }
  }
  if (target_w == 0 || target_h == 0) {
    eastl::array<int, 2> drawable =
        g_runtime_global_context.m_window_system->getDrawableSize();
    target_w = static_cast<uint32_t>(drawable[0] > 0 ? drawable[0] : 1);
    target_h = static_cast<uint32_t>(drawable[1] > 0 ? drawable[1] : 1);
  }
  // Guard against degenerate sizes during transient layouts (folded panels,
  // window minimised, etc.) to avoid creating tiny render targets that get
  // immediately invalidated on the next frame.
  constexpr uint32_t k_min_viewport_extent = 16u;
  if (target_w < k_min_viewport_extent) target_w = k_min_viewport_extent;
  if (target_h < k_min_viewport_extent) target_h = k_min_viewport_extent;

  g_runtime_global_context.m_render_system->tick(delta_time, target_w, target_h);
  return true;
}

const float BlunderEngine::s_fps_alpha = 1.f / 100;
void BlunderEngine::calculateFPS(float delta_time) {
  m_frame_count++;

  if (m_frame_count == 1) {
    m_average_duration = delta_time;
  } else {
    m_average_duration =
        m_average_duration * (1 - s_fps_alpha) + delta_time * s_fps_alpha;
  }

  m_fps = static_cast<int>(1.f / m_average_duration);
}
}  // namespace Blunder
