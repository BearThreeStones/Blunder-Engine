#pragma once

#include <cstdint>
#include <optional>

#include <glm/gtc/quaternion.hpp>
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

enum class TranslateModalEntry : uint8_t {
  handle,
  grab,
};

enum class TranslateModalAxisKey : uint8_t {
  x,
  y,
  z,
};

enum class TranslateModalConstraintOrientation : uint8_t {
  global,
  local,
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

bool translateSessionShowsPlaneHandle(ManipulatorAxis active,
                                      ManipulatorAxis plane);
bool translateSessionShowsCenterHandle();
uint32_t translateSessionGuideAxisCount(ManipulatorAxis active);
uint32_t translateSessionGuideAxes(ManipulatorAxis active,
                                   ManipulatorAxis out_axes[2]);
ManipulatorAxis translateSessionOriginColorAxis(ManipulatorAxis active);
bool translateModalConfirmsOnMousePress(TranslateModalEntry entry);
bool translateModalConfirmsOnMouseRelease(TranslateModalEntry entry);

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
                       const TranslateModalCameraState& camera,
                       const glm::quat& session_start_rotation =
                           glm::identity<glm::quat>());
  void beginFromHandle(ManipulatorAxis axis, const GizmoBasis& basis,
                       const glm::vec2& pointer_position,
                       const glm::vec3& object_position,
                       const EditorCamera& camera,
                       const glm::quat& session_start_rotation =
                           glm::identity<glm::quat>());
  void beginFromGrab(const glm::vec2& pointer_position,
                     const glm::vec3& object_position,
                     const TranslateModalCameraState& camera,
                     const glm::quat& session_start_rotation =
                         glm::identity<glm::quat>());
  void beginFromGrab(const glm::vec2& pointer_position,
                     const glm::vec3& object_position,
                     const EditorCamera& camera,
                     const glm::quat& session_start_rotation =
                         glm::identity<glm::quat>());
  void applyAxisConstraintKey(TranslateModalAxisKey key);
  void applyPlaneConstraintKey(TranslateModalAxisKey locked_axis);
  void onPointerMove(const glm::vec2& pointer_position,
                     const TranslateModalCameraState& camera);
  void onPointerMove(const glm::vec2& pointer_position,
                     const EditorCamera& camera);
  std::optional<glm::vec3> confirm();
  void cancel();

  bool isActive() const;
  ManipulatorAxis activeHandle() const;
  const GizmoBasis& dragStartBasis() const;
  glm::vec3 feedbackDelta() const;
  glm::vec3 feedbackPosition() const;
  TranslateModalEntry entryKind() const;
  bool isGrabEntry() const;
  TranslateModalConstraintOrientation constraintOrientation() const;
  bool isConstraintFree() const;

 private:
  enum class TranslateModalConstraintSlot : uint8_t {
    none,
    axis_x,
    axis_y,
    axis_z,
    plane_lock_x,
    plane_lock_y,
    plane_lock_z,
  };

  void rebuildConstraintBasis();
  void reprojectFeedbackFromPointer();
  void setGlobalAxisConstraint(TranslateModalAxisKey key);
  void setGlobalPlaneConstraint(TranslateModalAxisKey locked_axis);
  void advanceConstraintCycle(TranslateModalConstraintSlot slot,
                              ManipulatorAxis axis);

  static TranslateModalConstraintSlot axisSlotFor(TranslateModalAxisKey key);
  static TranslateModalConstraintSlot planeSlotFor(TranslateModalAxisKey key);
  static ManipulatorAxis manipulatorAxisFor(TranslateModalAxisKey key);
  static ManipulatorAxis manipulatorPlaneFor(TranslateModalAxisKey locked_axis);

  TranslateModalEntry m_entry{TranslateModalEntry::handle};
  ManipulatorAxis m_axis{ManipulatorAxis::last};
  GizmoBasis m_basis{};
  TranslateModalCameraState m_camera{};
  glm::vec2 m_start_pointer{0.0f};
  glm::vec2 m_current_pointer{0.0f};
  glm::vec3 m_object_position_at_begin{0.0f};
  glm::quat m_session_start_rotation{glm::identity<glm::quat>()};
  glm::vec3 m_feedback_delta{0.0f};
  TranslateModalConstraintSlot m_active_slot{TranslateModalConstraintSlot::none};
  TranslateModalConstraintOrientation m_orientation{
      TranslateModalConstraintOrientation::global};
  bool m_active{false};
};

}  // namespace Blunder
