#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/render/gizmo/translate_modal_session.h"
#include "runtime/function/render/gizmo/transform_gizmo_pick.h"

#include <cassert>
#include <cmath>
#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {

bool nearlyEqual(const glm::vec3& a, const glm::vec3& b,
                 const float epsilon = 1e-4f) {
  return glm::length(a - b) <= epsilon;
}

void expectVec3(const glm::vec3& actual, const glm::vec3& expected) {
  assert(nearlyEqual(actual, expected));
}

Blunder::GizmoBasis identityBasis() {
  Blunder::GizmoBasis basis{};
  basis.origin = glm::vec3(0.0f);
  basis.axis_x = glm::vec3(1.0f, 0.0f, 0.0f);
  basis.axis_y = glm::vec3(0.0f, 1.0f, 0.0f);
  basis.axis_z = glm::vec3(0.0f, 0.0f, 1.0f);
  return basis;
}

void screenDeltaUsesCameraRightAndUp() {
  const glm::vec3 delta = Blunder::screenDeltaToViewPlaneWorld(
      glm::vec3(0.0f, -10.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
      glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
      0.25f, glm::vec2(8.0f, -4.0f));

  expectVec3(delta, glm::vec3(2.0f, 0.0f, 1.0f));
}

void axisConstraintProjectsOntoHandleAxis() {
  const glm::vec3 delta = Blunder::constrainTranslationDelta(
      glm::vec3(3.0f, 4.0f, 5.0f), Blunder::ManipulatorAxis::trans_y,
      identityBasis(), glm::vec3(1.0f, 0.0f, 0.0f));

  expectVec3(delta, glm::vec3(0.0f, 4.0f, 0.0f));
}

void axisConstraintUsesViewAlignedFallbackWhenAxisFacesCamera() {
  const glm::vec3 delta = Blunder::constrainTranslationDelta(
      glm::vec3(3.0f, 4.0f, 5.0f), Blunder::ManipulatorAxis::trans_y,
      identityBasis(), glm::vec3(0.0f, -1.0f, 0.0f));

  expectVec3(delta, glm::vec3(3.0f, 0.0f, 5.0f));
}

void planeConstraintProjectsOntoInfinitePlane() {
  const glm::vec3 delta = Blunder::constrainTranslationDelta(
      glm::vec3(3.0f, 4.0f, 5.0f), Blunder::ManipulatorAxis::trans_zx,
      identityBasis(), glm::vec3(0.0f, -1.0f, 0.0f));

  expectVec3(delta, glm::vec3(3.0f, 0.0f, 5.0f));
}

void pickTranslationPrefersPlaneHandleOverNearbyAxes() {
  // Plane handles sit in axis corners. A ray through the XY plane center is
  // also within the generous axis pick threshold — plane hits must win.
  const float group_scale = 1.0f;
  const float plane_scale = Blunder::computeGizmoHandleScale(
      group_scale, Blunder::ManipulatorAxis::trans_xy);
  const float plane_offset =
      plane_scale * Blunder::TransformGizmoMetrics::k_mesh_plane_center_offset;
  const glm::vec3 plane_center(plane_offset, plane_offset, 0.0f);

  Blunder::TransformGizmoPickContext ctx{};
  ctx.basis = identityBasis();
  ctx.group_scale = group_scale;
  ctx.camera_position = glm::vec3(plane_center.x, plane_center.y, 10.0f);
  ctx.camera_forward = glm::vec3(0.0f, 0.0f, -1.0f);
  ctx.ray.origin = ctx.camera_position;
  ctx.ray.direction = glm::normalize(plane_center - ctx.camera_position);

  const std::optional<Blunder::ManipulatorAxis> hit =
      Blunder::pickTranslationGizmoHandle(ctx);
  assert(hit.has_value());
  assert(*hit == Blunder::ManipulatorAxis::trans_xy);
}

void handlePlaneSessionMovesOnPointerDelta() {
  Blunder::TranslateModalCameraState camera{};
  camera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
  camera.right = glm::vec3(1.0f, 0.0f, 0.0f);
  camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.viewport_height_world_per_pixel = 0.1f;

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_xy, identityBasis(),
                          glm::vec2(10.0f, 10.0f), glm::vec3(1.0f, 2.0f, 3.0f),
                          camera);
  session.onPointerMove(glm::vec2(20.0f, 0.0f), camera);

  assert(session.isActive());
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_xy);
  // Screen delta (10, -10) → world (1, 1, 0) on view plane; XY keeps both.
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 1.0f, 0.0f));
  expectVec3(session.feedbackPosition(), glm::vec3(2.0f, 3.0f, 3.0f));
}

void centerConstraintKeepsViewPlaneDelta() {
  const glm::vec3 delta = Blunder::constrainTranslationDelta(
      glm::vec3(3.0f, 4.0f, 5.0f), Blunder::ManipulatorAxis::trans_c,
      identityBasis(), glm::vec3(0.0f, -1.0f, 0.0f));

  expectVec3(delta, glm::vec3(3.0f, 4.0f, 5.0f));
}

void axisDragHidesAllPlaneHandles() {
  assert(!Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_x, Blunder::ManipulatorAxis::trans_xy));
  assert(!Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_x, Blunder::ManipulatorAxis::trans_yz));
  assert(!Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_x, Blunder::ManipulatorAxis::trans_zx));
}

void planeDragKeepsOnlyActivePlaneHandle() {
  assert(Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_xy, Blunder::ManipulatorAxis::trans_xy));
  assert(!Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_xy, Blunder::ManipulatorAxis::trans_yz));
  assert(!Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_xy, Blunder::ManipulatorAxis::trans_zx));
}

void centerDragKeepsPlaneHandlesVisible() {
  assert(Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_c, Blunder::ManipulatorAxis::trans_xy));
  assert(Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_c, Blunder::ManipulatorAxis::trans_yz));
  assert(Blunder::translateSessionShowsPlaneHandle(
      Blunder::ManipulatorAxis::trans_c, Blunder::ManipulatorAxis::trans_zx));
}

void centerHandleIsHiddenDuringTranslateSession() {
  assert(!Blunder::translateSessionShowsCenterHandle());
}

void constraintFeedbackFollowsLiveConstraint() {
  assert(!Blunder::translateSessionShowsConstraintGuides(
      Blunder::ManipulatorAxis::trans_c));
  assert(!Blunder::translateSessionShowsOriginDot(
      Blunder::ManipulatorAxis::trans_c));

  assert(Blunder::translateSessionShowsConstraintGuides(
      Blunder::ManipulatorAxis::trans_x));
  assert(Blunder::translateSessionShowsOriginDot(
      Blunder::ManipulatorAxis::trans_x));
  assert(Blunder::translateSessionShowsConstraintGuides(
      Blunder::ManipulatorAxis::trans_xy));
  assert(Blunder::translateSessionShowsOriginDot(
      Blunder::ManipulatorAxis::trans_xy));
}

void handleGhostOnlyForHandleEntry() {
  assert(Blunder::translateSessionShowsHandleGhost(
      Blunder::TranslateModalEntry::handle));
  assert(!Blunder::translateSessionShowsHandleGhost(
      Blunder::TranslateModalEntry::grab));
}

void pressedHandleSurvivesLiveConstraintChange() {
  Blunder::TranslateModalCameraState camera{};
  camera.forward = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.right = glm::vec3(1.0f, 0.0f, 0.0f);
  camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
  camera.viewport_height_world_per_pixel = 0.1f;

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_y, identityBasis(),
                          glm::vec2(0.0f), glm::vec3(0.0f), camera);
  assert(session.pressedHandle() == Blunder::ManipulatorAxis::trans_y);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_y);

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_x);
  assert(session.pressedHandle() == Blunder::ManipulatorAxis::trans_y);

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_c);
  assert(session.pressedHandle() == Blunder::ManipulatorAxis::trans_y);
}

void grabSessionHasNoPressedHandle() {
  Blunder::TranslateModalCameraState camera{};
  camera.forward = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.right = glm::vec3(1.0f, 0.0f, 0.0f);
  camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
  camera.viewport_height_world_per_pixel = 0.1f;

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  assert(session.pressedHandle() == Blunder::ManipulatorAxis::last);
  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_z);
  assert(session.pressedHandle() == Blunder::ManipulatorAxis::last);
}

void guideAxisCountMatchesActiveConstraint() {
  Blunder::ManipulatorAxis axes[2] = {Blunder::ManipulatorAxis::last,
                                      Blunder::ManipulatorAxis::last};

  assert(Blunder::translateSessionGuideAxisCount(
             Blunder::ManipulatorAxis::trans_x) == 1u);
  assert(Blunder::translateSessionGuideAxes(
             Blunder::ManipulatorAxis::trans_x, axes) == 1u);
  assert(axes[0] == Blunder::ManipulatorAxis::trans_x);
  assert(axes[1] == Blunder::ManipulatorAxis::last);

  assert(Blunder::translateSessionGuideAxisCount(
             Blunder::ManipulatorAxis::trans_xy) == 2u);
  assert(Blunder::translateSessionGuideAxes(
             Blunder::ManipulatorAxis::trans_xy, axes) == 2u);
  assert(axes[0] == Blunder::ManipulatorAxis::trans_x);
  assert(axes[1] == Blunder::ManipulatorAxis::trans_y);

  assert(Blunder::translateSessionGuideAxisCount(
             Blunder::ManipulatorAxis::trans_c) == 0u);
  assert(Blunder::translateSessionGuideAxes(
             Blunder::ManipulatorAxis::trans_c, axes) == 0u);
  assert(axes[0] == Blunder::ManipulatorAxis::last);
  assert(axes[1] == Blunder::ManipulatorAxis::last);
}

void planeOriginColorUsesNormalAxis() {
  assert(Blunder::translateSessionOriginColorAxis(
             Blunder::ManipulatorAxis::trans_xy) ==
         Blunder::ManipulatorAxis::trans_z);
  assert(Blunder::translateSessionOriginColorAxis(
             Blunder::ManipulatorAxis::trans_yz) ==
         Blunder::ManipulatorAxis::trans_x);
  assert(Blunder::translateSessionOriginColorAxis(
             Blunder::ManipulatorAxis::trans_zx) ==
         Blunder::ManipulatorAxis::trans_y);
}

void sessionAppliesAccumulatedScreenDeltaUntilConfirm() {
  Blunder::TranslateModalCameraState camera{};
  camera.forward = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.right = glm::vec3(1.0f, 0.0f, 0.0f);
  camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
  camera.viewport_height_world_per_pixel = 0.1f;

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_c, identityBasis(),
                          glm::vec2(10.0f, 10.0f),
                          glm::vec3(1.0f, 2.0f, 3.0f), camera);
  session.onPointerMove(glm::vec2(15.0f, 5.0f), camera);

  assert(session.isActive());
  expectVec3(session.feedbackPosition(), glm::vec3(1.5f, 2.0f, 3.5f));
  const std::optional<glm::vec3> confirmed = session.confirm();
  assert(confirmed.has_value());
  expectVec3(*confirmed, glm::vec3(1.5f, 2.0f, 3.5f));
  assert(!session.isActive());
}

void grabSessionBeginsAsFreeViewPlaneMove() {
  Blunder::TranslateModalCameraState camera{};
  camera.forward = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.right = glm::vec3(1.0f, 0.0f, 0.0f);
  camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
  camera.viewport_height_world_per_pixel = 0.25f;

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(12.0f, 20.0f),
                        glm::vec3(4.0f, 5.0f, 6.0f), camera);
  session.onPointerMove(glm::vec2(16.0f, 12.0f), camera);

  assert(session.isActive());
  assert(session.entryKind() == Blunder::TranslateModalEntry::grab);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_c);
  expectVec3(session.dragStartBasis().origin, glm::vec3(4.0f, 5.0f, 6.0f));
  expectVec3(session.dragStartBasis().axis_x, glm::vec3(1.0f, 0.0f, 0.0f));
  expectVec3(session.dragStartBasis().axis_y, glm::vec3(0.0f, 1.0f, 0.0f));
  expectVec3(session.dragStartBasis().axis_z, glm::vec3(0.0f, 0.0f, 1.0f));
  expectVec3(session.feedbackPosition(), glm::vec3(5.0f, 5.0f, 8.0f));
}

void translateModalConfirmPolicyDependsOnEntryKind() {
  assert(Blunder::translateModalConfirmsOnMousePress(
      Blunder::TranslateModalEntry::grab));
  assert(!Blunder::translateModalConfirmsOnMouseRelease(
      Blunder::TranslateModalEntry::grab));

  assert(!Blunder::translateModalConfirmsOnMousePress(
      Blunder::TranslateModalEntry::handle));
  assert(Blunder::translateModalConfirmsOnMouseRelease(
      Blunder::TranslateModalEntry::handle));
}

void grabSessionCancelClearsActiveAndRestoresStartFeedback() {
  Blunder::TranslateModalCameraState camera{};
  camera.forward = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.right = glm::vec3(1.0f, 0.0f, 0.0f);
  camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
  camera.viewport_height_world_per_pixel = 0.5f;

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(2.0f, 3.0f, 4.0f),
                        camera);
  session.onPointerMove(glm::vec2(8.0f, -2.0f), camera);
  session.cancel();

  assert(!session.isActive());
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f));
  expectVec3(session.feedbackPosition(), glm::vec3(2.0f, 3.0f, 4.0f));
  assert(!session.confirm().has_value());
}

void sessionCancelClearsFeedback() {
  Blunder::TranslateModalCameraState camera{};
  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_x, identityBasis(),
                          glm::vec2(0.0f), glm::vec3(1.0f), camera);

  session.cancel();

  assert(!session.isActive());
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f));
  assert(!session.confirm().has_value());
}

Blunder::TranslateModalCameraState topDownCamera() {
  Blunder::TranslateModalCameraState camera{};
  camera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
  camera.right = glm::vec3(1.0f, 0.0f, 0.0f);
  camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.viewport_height_world_per_pixel = 0.1f;
  return camera;
}

void axisKeyCyclesGlobalLocalThenFree() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_x);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.isConstraintFree());
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_c);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));
}

void axisKeyYCyclesGlobalLocalThenFree() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::y);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_y);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -1.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::y);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -1.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::y);
  assert(session.isConstraintFree());
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_c);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));
}

Blunder::TranslateModalCameraState sideCamera() {
  Blunder::TranslateModalCameraState camera{};
  camera.forward = glm::vec3(1.0f, 0.0f, 0.0f);
  camera.right = glm::vec3(0.0f, 1.0f, 0.0f);
  camera.up = glm::vec3(0.0f, 0.0f, 1.0f);
  camera.viewport_height_world_per_pixel = 0.1f;
  return camera;
}

void axisKeyZCyclesGlobalLocalThenFree() {
  Blunder::TranslateModalCameraState camera = sideCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_z);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, 0.0f, -1.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, 0.0f, -1.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.isConstraintFree());
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_c);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, 1.0f, -1.0f));
}

void shiftXConstrainsToYzPlaneGlobally() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_yz);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -1.0f, 0.0f));
}

void shiftYConstrainsToZxPlaneGlobally() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::y);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_zx);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));
}

void localPlaneUsesSessionStartRotation() {
  Blunder::TranslateModalCameraState camera = topDownCamera();
  const glm::quat rotation =
      glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera, rotation);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::z);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));
}

void handleStartedSessionAppliesConstraintKey() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_c, identityBasis(),
                          glm::vec2(0.0f), glm::vec3(1.0f, 2.0f, 3.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::y);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_y);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -1.0f, 0.0f));
  expectVec3(session.feedbackPosition(), glm::vec3(1.0f, 1.0f, 3.0f));
}

void worldRotationFromMatrixStripsScale() {
  glm::mat4 world(1.0f);
  world = glm::rotate(world, glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
  world = glm::scale(world, glm::vec3(2.0f, 2.0f, 2.0f));
  const glm::quat rotation = Blunder::worldRotationFromMatrix(world);
  expectVec3(rotation * glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

void handleStartedGlobalAxisPressXCyclesToLocalThenFree() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_x, identityBasis(),
                          glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.isConstraintFree());
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));
}

void handleStartedLocalAxisPressXCyclesToFree() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromHandle(
      Blunder::ManipulatorAxis::trans_x, identityBasis(), glm::vec2(0.0f),
      glm::vec3(0.0f), camera, glm::identity<glm::quat>(),
      Blunder::TranslateModalConstraintOrientation::local);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.isConstraintFree());
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));
}

void differentAxisKeyResetsToGlobal() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::y);
  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::y);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -1.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_x);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));
}

void planeKeyConstrainsAndCyclesIndependently() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_xy);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.isConstraintFree());
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_yz);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -1.0f, 0.0f));
}

void axisKeyAfterPlaneKeyStartsGlobalAxis() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::z);
  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_x);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));
}

void grabThenConstrainReprojectsFromSessionStart() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(2.0f, 3.0f, 4.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 0.0f), camera);
  expectVec3(session.feedbackPosition(), glm::vec3(3.0f, 3.0f, 4.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_z);
  expectVec3(session.feedbackPosition(), glm::vec3(2.0f, 3.0f, 4.0f));
}

void localAxisUsesSessionStartRotation() {
  Blunder::TranslateModalCameraState camera = topDownCamera();
  // session_start_rotation is world-space (controller passes worldRotationFromMatrix).
  const glm::quat rotation =
      glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera, rotation);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -1.0f, 0.0f));
}

void constraintChangeUsesCurrentPointerNotAccumulatedDelta() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 0.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.onPointerMove(glm::vec2(0.0f, 10.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -1.0f, 0.0f));

  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, 0.0f, 0.0f));
}

void inactiveSessionIgnoresConstraintKeys() {
  Blunder::TranslateModalSession session;
  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  session.applyPlaneConstraintKey(Blunder::TranslateModalAxisKey::z);
  assert(!session.isActive());
}

void numericInputSetsAxisDistanceAlongGlobalX() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_x, identityBasis(),
                          glm::vec2(0.0f), glm::vec3(1.0f, 2.0f, 3.0f), camera);

  assert(session.appendNumericChar('2'));
  assert(session.hasNumericInput());
  assert(session.numericBuffer() == "2");
  expectVec3(session.feedbackDelta(), glm::vec3(2.0f, 0.0f, 0.0f));
  expectVec3(session.feedbackPosition(), glm::vec3(3.0f, 2.0f, 3.0f));

  const std::optional<glm::vec3> confirmed = session.confirm();
  assert(confirmed.has_value());
  expectVec3(*confirmed, glm::vec3(3.0f, 2.0f, 3.0f));
}

void numericInputSupportsNegativeAndDecimal() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_y, identityBasis(),
                          glm::vec2(0.0f), glm::vec3(0.0f), camera);

  assert(session.appendNumericChar('-'));
  assert(session.appendNumericChar('2'));
  assert(session.appendNumericChar('.'));
  assert(session.appendNumericChar('5'));
  assert(session.numericBuffer() == "-2.5");
  expectVec3(session.feedbackDelta(), glm::vec3(0.0f, -2.5f, 0.0f));
}

void numericInputIgnoresPointerMotionUntilCleared() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_x, identityBasis(),
                          glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  assert(session.appendNumericChar('2'));
  expectVec3(session.feedbackDelta(), glm::vec3(2.0f, 0.0f, 0.0f));

  session.onPointerMove(glm::vec2(50.0f, -20.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(2.0f, 0.0f, 0.0f));
}

void clearingNumericRestoresPointerDrivenMotion() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromHandle(Blunder::ManipulatorAxis::trans_x, identityBasis(),
                          glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  assert(session.appendNumericChar('2'));
  expectVec3(session.feedbackDelta(), glm::vec3(2.0f, 0.0f, 0.0f));

  session.onPointerMove(glm::vec2(50.0f, -20.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(2.0f, 0.0f, 0.0f));

  assert(session.backspaceNumeric());
  assert(!session.hasNumericInput());
  expectVec3(session.feedbackDelta(), glm::vec3(5.0f, 0.0f, 0.0f));

  assert(session.appendNumericChar('3'));
  expectVec3(session.feedbackDelta(), glm::vec3(3.0f, 0.0f, 0.0f));

  session.clearNumeric();
  assert(!session.hasNumericInput());
  expectVec3(session.feedbackDelta(), glm::vec3(5.0f, 0.0f, 0.0f));
}

void nearestAxisPicksHorizontalDeltaAsX() {
  const Blunder::TranslateModalCameraState camera = topDownCamera();
  const Blunder::GizmoBasis basis = identityBasis();
  assert(Blunder::nearestProjectedAxis(basis.origin, basis, camera,
                                       glm::vec2(120.0f, 0.0f)) ==
         Blunder::ManipulatorAxis::trans_x);
}

void nearestAxisPicksVerticalDeltaAsY() {
  const Blunder::TranslateModalCameraState camera = topDownCamera();
  const Blunder::GizmoBasis basis = identityBasis();
  assert(Blunder::nearestProjectedAxis(basis.origin, basis, camera,
                                       glm::vec2(0.0f, 80.0f)) ==
         Blunder::ManipulatorAxis::trans_y);
  assert(Blunder::nearestProjectedAxis(basis.origin, basis, camera,
                                       glm::vec2(0.0f, -80.0f)) ==
         Blunder::ManipulatorAxis::trans_y);
}

void nearestAxisPicksZFromSideView() {
  const Blunder::TranslateModalCameraState camera = sideCamera();
  const Blunder::GizmoBasis basis = identityBasis();
  assert(Blunder::nearestProjectedAxis(basis.origin, basis, camera,
                                       glm::vec2(0.0f, 60.0f)) ==
         Blunder::ManipulatorAxis::trans_z);
}

void mmbPickCommitsGlobalAxisAndContinuesSession() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 10.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, -1.0f, 0.0f));

  session.beginMmbAxisPick(glm::vec2(20.0f, 20.0f));
  assert(session.isMmbAxisPicking());
  session.updateMmbAxisPick(glm::vec2(120.0f, 25.0f), camera);
  assert(session.mmbPickNearestAxis() == Blunder::ManipulatorAxis::trans_x);

  const Blunder::ManipulatorAxis committed = session.endMmbAxisPick();
  assert(committed == Blunder::ManipulatorAxis::trans_x);
  assert(!session.isMmbAxisPicking());
  assert(session.isActive());
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_x);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::global);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));
}

void mmbPickUsesLocalProjectionBasisWhenOrientationLocal() {
  Blunder::TranslateModalCameraState camera = topDownCamera();
  const glm::quat rotation =
      glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera, rotation);
  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  session.applyAxisConstraintKey(Blunder::TranslateModalAxisKey::x);
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);

  session.beginMmbAxisPick(glm::vec2(0.0f, 0.0f));
  session.updateMmbAxisPick(glm::vec2(0.0f, 100.0f), camera);
  assert(session.mmbPickNearestAxis() == Blunder::ManipulatorAxis::trans_x);

  session.endMmbAxisPick();
  assert(session.constraintOrientation() ==
         Blunder::TranslateModalConstraintOrientation::local);
  assert(session.activeHandle() == Blunder::ManipulatorAxis::trans_x);
}

void mmbPickBlocksPointerMotionUntilRelease() {
  Blunder::TranslateModalCameraState camera = topDownCamera();

  Blunder::TranslateModalSession session;
  session.beginFromGrab(glm::vec2(0.0f), glm::vec3(0.0f), camera);
  session.onPointerMove(glm::vec2(10.0f, 0.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.beginMmbAxisPick(glm::vec2(0.0f, 0.0f));
  session.onPointerMove(glm::vec2(50.0f, 0.0f), camera);
  expectVec3(session.feedbackDelta(), glm::vec3(1.0f, 0.0f, 0.0f));

  session.updateMmbAxisPick(glm::vec2(50.0f, 0.0f), camera);
  session.endMmbAxisPick();
  expectVec3(session.feedbackDelta(), glm::vec3(5.0f, 0.0f, 0.0f));
}

}  // namespace

int main() {
  screenDeltaUsesCameraRightAndUp();
  axisConstraintProjectsOntoHandleAxis();
  axisConstraintUsesViewAlignedFallbackWhenAxisFacesCamera();
  planeConstraintProjectsOntoInfinitePlane();
  pickTranslationPrefersPlaneHandleOverNearbyAxes();
  handlePlaneSessionMovesOnPointerDelta();
  centerConstraintKeepsViewPlaneDelta();
  axisDragHidesAllPlaneHandles();
  planeDragKeepsOnlyActivePlaneHandle();
  centerDragKeepsPlaneHandlesVisible();
  centerHandleIsHiddenDuringTranslateSession();
  constraintFeedbackFollowsLiveConstraint();
  handleGhostOnlyForHandleEntry();
  pressedHandleSurvivesLiveConstraintChange();
  grabSessionHasNoPressedHandle();
  guideAxisCountMatchesActiveConstraint();
  planeOriginColorUsesNormalAxis();
  sessionAppliesAccumulatedScreenDeltaUntilConfirm();
  grabSessionBeginsAsFreeViewPlaneMove();
  translateModalConfirmPolicyDependsOnEntryKind();
  grabSessionCancelClearsActiveAndRestoresStartFeedback();
  sessionCancelClearsFeedback();
  axisKeyCyclesGlobalLocalThenFree();
  axisKeyYCyclesGlobalLocalThenFree();
  axisKeyZCyclesGlobalLocalThenFree();
  shiftXConstrainsToYzPlaneGlobally();
  shiftYConstrainsToZxPlaneGlobally();
  localPlaneUsesSessionStartRotation();
  handleStartedSessionAppliesConstraintKey();
  worldRotationFromMatrixStripsScale();
  handleStartedGlobalAxisPressXCyclesToLocalThenFree();
  handleStartedLocalAxisPressXCyclesToFree();
  differentAxisKeyResetsToGlobal();
  planeKeyConstrainsAndCyclesIndependently();
  axisKeyAfterPlaneKeyStartsGlobalAxis();
  grabThenConstrainReprojectsFromSessionStart();
  localAxisUsesSessionStartRotation();
  constraintChangeUsesCurrentPointerNotAccumulatedDelta();
  inactiveSessionIgnoresConstraintKeys();
  numericInputSetsAxisDistanceAlongGlobalX();
  numericInputSupportsNegativeAndDecimal();
  numericInputIgnoresPointerMotionUntilCleared();
  clearingNumericRestoresPointerDrivenMotion();
  nearestAxisPicksHorizontalDeltaAsX();
  nearestAxisPicksVerticalDeltaAsY();
  nearestAxisPicksZFromSideView();
  mmbPickCommitsGlobalAxisAndContinuesSession();
  mmbPickUsesLocalProjectionBasisWhenOrientationLocal();
  mmbPickBlocksPointerMotionUntilRelease();
  return 0;
}
