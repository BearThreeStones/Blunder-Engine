#include "runtime/engine.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/event/application_event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/core/layer/layer.h"
#include "runtime/core/layer/layer_stack.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/input/input_system.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/scene/scene_render_bridge.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/resource/content_browser/content_browser_system.h"
#include "runtime/function/editor/editor_scene_edit_system.h"


#include <SDL3/SDL.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <SDL3/SDL_system.h>
#endif

namespace Blunder {
bool g_is_editor_mode{false};
eastl::unordered_set<eastl::string> g_editor_tick_component_types{};

namespace {

constexpr const char* k_startup_scene_path = "assets/Scenes/root.scene.asset";

void activateEditorScene(const eastl::string& virtual_path) {
  if (g_runtime_global_context.m_editor_scene_edit) {
    if (!g_runtime_global_context.m_editor_scene_edit->openScene(virtual_path)) {
      return;
    }
  } else if (!g_runtime_global_context.m_scene_system || virtual_path.empty()) {
    return;
  } else {
    const eastl::shared_ptr<SceneInstance> instance =
        g_runtime_global_context.m_scene_system->loadScene(virtual_path);
    if (!instance) {
      LOG_ERROR("[BlunderEngine] failed to load scene '{}'", virtual_path.c_str());
      return;
    }
    g_runtime_global_context.m_scene_system->setActiveInstance(instance.get());
  }

  if (g_runtime_global_context.m_render_system &&
      g_runtime_global_context.m_scene_system) {
    syncSceneToRender(g_runtime_global_context.m_render_system.get(),
                      g_runtime_global_context.m_scene_system->getActiveInstance());
  }
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->refreshEditorScenePanels();
  }
}

}  // namespace

namespace {

#if defined(_WIN32)
bool SDLCALL win32ModalResizeMessageHook(void* userdata, MSG* msg) {
  if (userdata == nullptr || msg == nullptr) {
    return true;
  }
  auto* engine = static_cast<BlunderEngine*>(userdata);
  switch (msg->message) {
    case WM_ENTERSIZEMOVE:
      engine->onWin32ModalResizeBegin();
      break;
    case WM_EXITSIZEMOVE:
      engine->onWin32ModalResizeEnd();
      break;
    case WM_SIZING:
      if (g_runtime_global_context.m_slint_system) {
        g_runtime_global_context.m_slint_system->noteWin32SizingTick();
      }
      break;
    case WM_SIZE: {
      if (msg->wParam == SIZE_MINIMIZED ||
          !g_runtime_global_context.m_slint_system) {
        break;
      }
      int client_w = static_cast<int>(LOWORD(msg->lParam));
      int client_h = static_cast<int>(HIWORD(msg->lParam));
      if (g_runtime_global_context.m_window_system) {
        void* hwnd = g_runtime_global_context.m_window_system->getNativeWin32Hwnd();
        if (hwnd != nullptr) {
          RECT client_rect{};
          if (GetClientRect(static_cast<HWND>(hwnd), &client_rect)) {
            const int rect_w =
                static_cast<int>(client_rect.right - client_rect.left);
            const int rect_h =
                static_cast<int>(client_rect.bottom - client_rect.top);
            if (rect_w > 0) {
              client_w = eastl::max(client_w, rect_w);
            }
            if (rect_h > 0) {
              client_h = eastl::max(client_h, rect_h);
            }
          }
        }
        if (client_w <= 0 || client_h <= 0) {
          const eastl::array<int, 2> px =
              g_runtime_global_context.m_window_system->getDrawableSize();
          client_w = px[0];
          client_h = px[1];
        }
      }
      g_runtime_global_context.m_slint_system->notifyWin32ClientAreaChanged(
          static_cast<uintptr_t>(msg->wParam), client_w, client_h);
      break;
    }
    default:
      break;
  }
  return true;
}
#endif

}  // namespace

void BlunderEngine::onWin32ModalResizeBegin() {
  if (g_runtime_global_context.m_window_system) {
    g_runtime_global_context.m_window_system->setWin32ModalSizeLoopActive(true);
  }
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->setWin32SizeModalActive(true);
  }
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->notifyWindowResizeActivity();
  }
}

void BlunderEngine::onWin32ModalResizeEnd() {
  if (g_runtime_global_context.m_window_system) {
    g_runtime_global_context.m_window_system->setWin32ModalSizeLoopActive(false);
  }
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->setWin32SizeModalActive(false);
  }
  m_pending_finalize_window_resize = true;
}

void BlunderEngine::finalizePendingWindowResize() {
  if (!m_pending_finalize_window_resize) {
    return;
  }
  m_pending_finalize_window_resize = false;

  SlintSystem* slint = g_runtime_global_context.m_slint_system.get();
  if (slint) {
    // Layout commit only; keep resize cooldown so rendererTick/composite stay
    // deferred for subsequent frames (avoids one mega-stall frame on release).
    slint->finishLiveResizeModal();
  }
}

void BlunderEngine::processSdlEvent(const SDL_Event& event) {
  WindowSystem* window_system = g_runtime_global_context.m_window_system.get();
  SlintSystem* slint_system = g_runtime_global_context.m_slint_system.get();
  if (!window_system) {
    return;
  }
  const bool route_to_layers =
      slint_system == nullptr || slint_system->shouldRouteMouseToInputLayers(event);
  window_system->dispatchApplicationEvent(event, route_to_layers);
}

void BlunderEngine::startEngine() {
  g_runtime_global_context.startSystems();

  g_runtime_global_context.m_window_system->setEventCallback(
      [](Event& e) { onEvent(e); });

#if defined(_WIN32)
  SDL_SetWindowsMessageHook(win32ModalResizeMessageHook, this);
  LOG_INFO("[BlunderEngine] Win32 modal resize message hook installed");
#endif

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

  // Engine-level event handling
  EventDispatcher dispatcher(e);

  dispatcher.dispatch<WindowCloseEvent>([](WindowCloseEvent& event) {
    LOG_INFO("[Event] Window close requested");
    return true;
  });

  dispatcher.dispatch<KeyPressedEvent>([](KeyPressedEvent& event) {
    if (!event.isRepeat() && event.isCtrlDown() &&
        event.getKeyCode() == SDLK_S &&
        g_runtime_global_context.m_editor_scene_edit) {
      g_runtime_global_context.m_editor_scene_edit->saveActiveScene();
      return true;
    }
    return false;
  });

  dispatcher.dispatch<MouseButtonReleasedEvent>(
      [](MouseButtonReleasedEvent& event) {
        if (event.getMouseButton() != SDL_BUTTON_LEFT ||
            !g_runtime_global_context.m_slint_system) {
          return false;
        }
        if (!event.hasMousePosition()) {
          return false;
        }
        g_runtime_global_context.m_slint_system->trySelectContentBrowserTreeFolder(
            event.getX(), event.getY());
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

  if (g_runtime_global_context.m_content_browser) {
    const ContentBrowserRefreshStats stats =
        g_runtime_global_context.m_content_browser->refresh();
    LOG_INFO(
        "[BlunderEngine] content index: {} entries (thumbnails: {} generated, "
        "{} cached, {} skipped, {} failed)",
        stats.entry_count, stats.thumbnails_generated, stats.thumbnails_cached,
        stats.thumbnails_skipped, stats.thumbnails_failed);

    if (g_runtime_global_context.m_slint_system) {
      g_runtime_global_context.m_slint_system->syncContentBrowser();
    }
  }

  if (g_runtime_global_context.m_scene_system) {
    activateEditorScene(k_startup_scene_path);
    if (SceneInstance* active =
            g_runtime_global_context.m_scene_system->getActiveInstance()) {
      LOG_INFO("[BlunderEngine] active scene '{}' (entities={})",
               active->getSourcePath().c_str(), active->getEntityCount());
    }
  }
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
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      processSdlEvent(event);
    }
    const float delta_time = calculateDeltaTime();
    if (!tickOneFrame(delta_time)) {
      break;
    }
  }
}

float BlunderEngine::calculateDeltaTime() { return m_frame_timer.tick(); }

bool BlunderEngine::tickOneFrame(float delta_time) {
  finalizePendingWindowResize();

  g_runtime_global_context.m_memory_system.beginFrame();

  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->beginContentBrowserInputFrame();
  }

  SlintSystem* slint_system = g_runtime_global_context.m_slint_system.get();

  // Layout/viewport cooldown is updated in beginFrame — evaluate defer after that
  // so rendererTick does not run on the same frame the dock splitter resizes the viewport.
  if (slint_system) {
    slint_system->beginFrame();
  }

  const bool defer_heavy =
      slint_system != nullptr && slint_system->shouldDeferHeavyFrameWork();

  static bool s_was_defer_heavy = false;
  if (s_was_defer_heavy && !defer_heavy) {
    m_skip_renderer_after_defer = true;
  }
  s_was_defer_heavy = defer_heavy;

  if (!defer_heavy) {
    if (g_runtime_global_context.m_content_browser &&
        g_runtime_global_context.m_content_browser->tickFileWatch()) {
      g_runtime_global_context.m_content_browser->refresh();
      if (slint_system) {
        slint_system->syncContentBrowser();
      }
    }

    g_runtime_global_context.m_input_system->tick();

    for (Layer* layer : *g_runtime_global_context.m_layer_stack) {
      layer->onUpdate(delta_time);
    }

    if (g_runtime_global_context.m_scene_system) {
      g_runtime_global_context.m_scene_system->tick(delta_time);
      if (g_runtime_global_context.m_render_system &&
          g_runtime_global_context.m_scene_system->getActiveInstance()) {
        syncSceneToRender(g_runtime_global_context.m_render_system.get(),
                          g_runtime_global_context.m_scene_system->getActiveInstance());
      }
    }
  }

  calculateFPS(delta_time);

  if (slint_system) {
    if (!defer_heavy && !m_skip_renderer_after_defer) {
      rendererTick(delta_time);
    }
    if (m_skip_renderer_after_defer) {
      m_skip_renderer_after_defer = false;
    }
    slint_system->endFrame();
  }

#ifdef ENABLE_PHYSICS_DEBUG_RENDERER
  if (!defer_heavy && g_runtime_global_context.m_physics_manager) {
    g_runtime_global_context.m_physics_manager->renderPhysicsWorld(delta_time);
  }
#endif

  if (!defer_heavy) {
    g_runtime_global_context.m_window_system->setTitle(
        std::string("Blunder - " + std::to_string(getFPS()) + " FPS").c_str());
  }

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
  if (slint_system && slint_system->shouldDeferHeavyFrameWork()) {
    return true;
  }
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
