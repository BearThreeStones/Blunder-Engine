#include "runtime/engine.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/event/application_event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/core/layer/layer.h"
#include "runtime/core/layer/layer_stack.h"
// #include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/imgui/imgui_system.h"
#include "runtime/platform/input/input_system.h"
// #include "runtime/function/particle/particle_manager.h"
// #include "runtime/function/physics/physics_manager.h"
// #include "runtime/function/render/debugdraw/debug_draw_manager.h"
#include "runtime/function/render/render_system.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {
bool g_is_editor_mode{false};
eastl::unordered_set<eastl::string> g_editor_tick_component_types{};

void BlunderEngine::startEngine() {
  g_runtime_global_context.startSystems();

  // Register event callback for testing
  g_runtime_global_context.m_window_system->setEventCallback(
      [](Event& e) { onEvent(e); });

  LOG_INFO("engine start");
}

void BlunderEngine::onEvent(Event& e) {
  // Propagate to layers in reverse order (overlays first)
  // This allows overlays (like ImGui) to consume events before game layers
  auto& layer_stack = g_runtime_global_context.m_layer_stack;
  for (auto it = layer_stack->rbegin(); it != layer_stack->rend(); ++it) {
    if (e.handled) break;
    (*it)->onEvent(e);
  }

  // Log events for debugging (skip mouse move to reduce spam)
  if (e.getEventType() != EventType::MouseMoved && !e.handled) {
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
      LOG_DEBUG("[Event] Key pressed: {}", event.getKeyCode());
    }
    return false;
  });
}

void BlunderEngine::shutdownEngine() {
  LOG_INFO("engine shutdown");

  g_runtime_global_context.shutdownSystems();
}

void BlunderEngine::initialize() {}
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

  // 开始新的一帧 ImGui 渲染，允许各层在 onImGuiRender 中绘制 UI
  g_runtime_global_context.m_imgui_system->beginFrame();

  // 更新所有层（正向迭代：Layer1 → OverlayN）
  for (Layer* layer : *g_runtime_global_context.m_layer_stack) {
    layer->onUpdate(delta_time);
  }

  // ImGui rendering for all layers
  for (Layer* layer : *g_runtime_global_context.m_layer_stack) {
    layer->onImGuiRender();
  }

  // End ImGui frame and render
  g_runtime_global_context.m_imgui_system->endFrame();

  // logicalTick(delta_time);
  calculateFPS(delta_time);

  // single thread
  // exchange data between logic and render contexts

  rendererTick(delta_time);

#ifdef ENABLE_PHYSICS_DEBUG_RENDERER
  g_runtime_global_context.m_physics_manager->renderPhysicsWorld(delta_time);
#endif

  g_runtime_global_context.m_window_system->pollEvents();

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
  g_runtime_global_context.m_render_system->tick(
      delta_time, [](VkCommandBuffer cmd) {
        g_runtime_global_context.m_imgui_system->render(cmd);
      });
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
