#pragma once

#include <cstdint>

#include <glm/vec2.hpp>

#include "EASTL/vector.h"

#include "runtime/function/render/pick/hybrid_gpu_pick_system.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class RenderSystem;
class SceneInstance;

/// Editor viewport mesh picking (async hybrid GPU peel).
class ViewportPickSystem final {
 public:
  static constexpr float k_click_cycle_pixel_tolerance = 3.0f;
  static constexpr float k_click_drag_threshold_px = 5.0f;
  static constexpr int k_max_peel_count = 16;

  void onViewportLeftPressed(float window_x, float window_y);
  void onViewportLeftReleased(float window_x, float window_y, uint16_t modifier_flags);
  void onViewportPointerMoved(float window_x, float window_y);
  void onPiercingMenuRequest(float window_x, float window_y, uint16_t modifier_flags);
  void onCameraInteractionStarted();
  void suppressNextLeftReleasePick();

  void deliverPickResult(const PickResult& result, SceneInstance& scene);

  /// Called at the start of each `tickVulkan` to flush a deferred multi_peel request.
  void tickDeferredPickRequest();

 private:
  void applySelection(EntityId entity_id, uint16_t modifier_flags) const;
  void notifyEditorUi() const;
  void requestPick(float window_x, float window_y, uint16_t modifier_flags,
                   PickRequestKind kind);
  EntityId pickWithCyclingFromResult(const PickResult& result, SceneInstance& scene);

  glm::vec2 m_last_click_pixel{-1000.0f, -1000.0f};
  eastl::vector<EntityId> m_last_peel_hits;
  size_t m_cycle_index{0};

  bool m_left_pressed_in_viewport{false};
  glm::vec2 m_left_press_origin{0.0f, 0.0f};
  bool m_left_drag_exceeded{false};
  bool m_camera_interaction_since_left_press{false};
  bool m_suppress_next_left_release_pick{false};

  bool m_deferred_multi_peel{false};
  bool m_deferred_multi_peel_wait_one_tick{false};
  float m_deferred_window_x{0.0f};
  float m_deferred_window_y{0.0f};
  uint16_t m_deferred_modifier_flags{0};
};

}  // namespace Blunder
