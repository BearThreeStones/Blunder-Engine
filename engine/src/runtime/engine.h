#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <filesystem>

#include "EASTL/string.h"
#include "EASTL/unordered_set.h"

#include "runtime/core/base/high_precision_timer.h"
#include "runtime/core/event/event.h"

namespace Blunder {

extern bool g_is_editor_mode;
extern eastl::unordered_set<eastl::string> g_editor_tick_component_types;

class Layer;

class BlunderEngine {
  // friend class BlunderEditor;

  static const float s_fps_alpha;

 public:
  void startEngine();
  void shutdownEngine();

  void initialize();
  void clear();

  bool isQuit() const { return m_is_quit; }
  void run();
  bool tickOneFrame(float delta_time);

  /// SDL_AppEvent / legacy run-loop event delivery (does not pump SDL).
  void processSdlEvent(const SDL_Event& event);

  float calculateDeltaTime();

  void onWin32ModalResizeBegin();
  void onWin32ModalResizeEnd();

  void finalizePendingWindowResize();

  int getFPS() const { return m_fps; }

  // Event handling
  static void onEvent(Event& e);

  // Layer management
  void pushLayer(Layer* layer);
  void pushOverlay(Layer* overlay);

 protected:
  // void logicalTick(float delta_time);
  bool rendererTick(float delta_time);

  void calculateFPS(float delta_time);

 protected:
  bool m_is_quit{false};

  HighPrecisionTimer m_frame_timer{};

  float m_average_duration{0.f};
  int m_frame_count{0};
  int m_fps{0};

  bool m_pending_finalize_window_resize{false};
  bool m_skip_renderer_after_defer{false};
};

}  // namespace Blunder
