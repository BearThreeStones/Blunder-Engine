#pragma once

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "runtime/function/render/gizmo/gizmo_math.h"

namespace Blunder {

class EditorCamera;

struct TranslateModalCameraState {
  glm::vec3 position{0.0f};
  glm::vec3 forward{0.0f, 0.0f, -1.0f};
  glm::vec3 right{1.0f, 0.0f, 0.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  float viewport_height_world_per_pixel{1.0f};
};

glm::vec3 screenDeltaToViewPlaneWorld(const glm::vec3& camera_position,
                                      const glm::vec3& camera_forward,
                                      const glm::vec3& camera_right,
                                      const glm::vec3& camera_up,
                                      float viewport_height_world_per_pixel,
                                      const glm::vec2& pixel_delta);

glm::vec3 constrainTranslationDelta(const glm::vec3& free_delta,
                                    ManipulatorAxis axis,
                                    const GizmoBasis& basis,
                                    const glm::vec3& view_forward);

float viewportHeightWorldPerPixel(const EditorCamera& camera);
float viewportHeightWorldPerPixel(const EditorCamera& camera,
                                  const glm::vec3& pivot_position);
TranslateModalCameraState cameraStateFromEditorCamera(const EditorCamera& camera);
TranslateModalCameraState cameraStateFromEditorCamera(
    const EditorCamera& camera, const glm::vec3& pivot_position);

class TranslateModalSession final {
 public:
  void beginFromHandle(ManipulatorAxis axis, const GizmoBasis& basis,
                       const glm::vec2& pointer_position,
                       const glm::vec3& object_position,
                       const TranslateModalCameraState& camera);
  void beginFromHandle(ManipulatorAxis axis, const GizmoBasis& basis,
                       const glm::vec2& pointer_position,
                       const glm::vec3& object_position,
                       const EditorCamera& camera);
  void onPointerMove(const glm::vec2& pointer_position,
                     const TranslateModalCameraState& camera);
  void onPointerMove(const glm::vec2& pointer_position,
                     const EditorCamera& camera);
  std::optional<glm::vec3> confirm();
  void cancel();

  bool isActive() const;
  ManipulatorAxis activeHandle() const;
  glm::vec3 feedbackDelta() const;
  glm::vec3 feedbackPosition() const;

 private:
  ManipulatorAxis m_axis{ManipulatorAxis::last};
  GizmoBasis m_basis{};
  TranslateModalCameraState m_camera{};
  glm::vec2 m_start_pointer{0.0f};
  glm::vec3 m_object_position_at_begin{0.0f};
  glm::vec3 m_feedback_delta{0.0f};
  bool m_active{false};
};

}  // namespace Blunder
