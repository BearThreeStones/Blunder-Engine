#include "runtime/function/render/gizmo/translate_modal_session.h"

#include <cassert>
#include <cmath>
#include <optional>

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

}  // namespace

int main() {
  screenDeltaUsesCameraRightAndUp();
  axisConstraintProjectsOntoHandleAxis();
  axisConstraintUsesViewAlignedFallbackWhenAxisFacesCamera();
  planeConstraintProjectsOntoInfinitePlane();
  centerConstraintKeepsViewPlaneDelta();
  axisDragHidesAllPlaneHandles();
  planeDragKeepsOnlyActivePlaneHandle();
  centerDragKeepsPlaneHandlesVisible();
  centerHandleIsHiddenDuringTranslateSession();
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
  differentAxisKeyResetsToGlobal();
  planeKeyConstrainsAndCyclesIndependently();
  axisKeyAfterPlaneKeyStartsGlobalAxis();
  grabThenConstrainReprojectsFromSessionStart();
  localAxisUsesSessionStartRotation();
  constraintChangeUsesCurrentPointerNotAccumulatedDelta();
  inactiveSessionIgnoresConstraintKeys();
  return 0;
}
