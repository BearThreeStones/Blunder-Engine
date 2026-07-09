#include "runtime/function/render/gizmo/translate_modal_session.h"

#include <cassert>
#include <cmath>
#include <optional>

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

void centerHandleIsHiddenDuringTranslateSession() {
  assert(!Blunder::translateSessionShowsCenterHandle());
}

void guideAxisCountMatchesActiveConstraint() {
  assert(Blunder::translateSessionGuideAxisCount(
             Blunder::ManipulatorAxis::trans_y) == 1u);
  assert(Blunder::translateSessionGuideAxisCount(
             Blunder::ManipulatorAxis::trans_yz) == 2u);
  assert(Blunder::translateSessionGuideAxisCount(
             Blunder::ManipulatorAxis::trans_c) == 0u);
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

}  // namespace

int main() {
  screenDeltaUsesCameraRightAndUp();
  axisConstraintProjectsOntoHandleAxis();
  axisConstraintUsesViewAlignedFallbackWhenAxisFacesCamera();
  planeConstraintProjectsOntoInfinitePlane();
  centerConstraintKeepsViewPlaneDelta();
  axisDragHidesAllPlaneHandles();
  planeDragKeepsOnlyActivePlaneHandle();
  centerHandleIsHiddenDuringTranslateSession();
  guideAxisCountMatchesActiveConstraint();
  planeOriginColorUsesNormalAxis();
  sessionAppliesAccumulatedScreenDeltaUntilConfirm();
  sessionCancelClearsFeedback();
  return 0;
}
