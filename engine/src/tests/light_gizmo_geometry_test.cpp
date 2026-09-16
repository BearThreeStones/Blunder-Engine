#include "runtime/function/render/overlay/light_gizmo_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>

#include "runtime/function/scene/gltf_unit_scale.h"

namespace {

constexpr float kEps = 1e-4f;

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_near(float actual, float expected, const char* label) {
  if (std::fabs(actual - expected) > 1e-3f) {
    std::fprintf(stderr, "FAIL %s: expected %.6f got %.6f\n", label, expected,
                 actual);
    ++g_failures;
  }
}

float maxRingRadius(const Blunder::LightGizmoShape& shape) {
  float max_r = 0.0f;
  Blunder::forEachLightGizmoSegmentLocal(shape, [&](const Blunder::Vec3& a,
                                                    const Blunder::Vec3& b) {
    max_r = std::max(max_r, glm::length(a));
    max_r = std::max(max_r, glm::length(b));
  });
  return max_r;
}

float minSpotBaseZ(const Blunder::LightGizmoShape& shape) {
  float min_z = 0.0f;
  Blunder::forEachLightGizmoSegmentLocal(shape, [&](const Blunder::Vec3& a,
                                                    const Blunder::Vec3& b) {
    min_z = std::min(min_z, a.z);
    min_z = std::min(min_z, b.z);
  });
  return min_z;
}

}  // namespace

int main() {
  using namespace Blunder;

  LightGizmoShape point{};
  point.kind = LightGizmoKind::point;
  point.range = 50.0f;
  expect_near(maxRingRadius(point), kLightGizmoPointDisplayRadius,
              "unselected point uses display radius not attenuation range");

  point.show_range = true;
  expect_true("selected point includes attenuation range",
              maxRingRadius(point) > 49.0f);

  LightGizmoShape spot{};
  spot.kind = LightGizmoKind::spot;
  spot.range = 18.0f;
  spot.outer_cone_degrees = 18.0f;
  expect_near(minSpotBaseZ(spot), -kLightGizmoSpotDisplayLength,
              "unselected spot cone uses display length");

  spot.show_range = true;
  expect_near(minSpotBaseZ(spot), -18.0f, "selected spot cone uses Unique range");

  Mat4 scaled(1.0f);
  scaled[0][0] = 2.0f;
  scaled[1][1] = 0.4f;
  scaled[2][2] = 3.0f;
  scaled[3] = Vec4(10.0f, 20.0f, 30.0f, 1.0f);
  const Mat4 point_world =
      makeLightGizmoWorldMatrix(scaled, LightGizmoKind::point);
  const Vec3 mapped = Vec3(point_world * Vec4(kLightGizmoPointDisplayRadius, 0.0f,
                                              0.0f, 1.0f));
  expect_near(mapped.x, 10.0f + kLightGizmoPointDisplayRadius,
              "point gizmo keeps translation and drops non-uniform scale x");
  expect_near(mapped.y, 20.0f,
              "point gizmo keeps translation and drops non-uniform scale y");
  expect_near(mapped.z, 30.0f,
              "point gizmo keeps translation and drops non-uniform scale z");

  const Mat4 spot_world =
      makeLightGizmoWorldMatrix(scaled, LightGizmoKind::spot);
  const Vec3 origin = Vec3(spot_world * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
  expect_near(origin.x, 10.0f, "spot gizmo origin x");
  expect_near(origin.y, 20.0f, "spot gizmo origin y");
  expect_near(origin.z, 30.0f, "spot gizmo origin z");
  const Vec3 axis = Vec3(spot_world * Vec4(0.0f, 0.0f, -1.0f, 0.0f));
  expect_near(glm::length(axis), 1.0f, "spot emit axis is unit after scale strip");

  const Vec3 meters(-6.5f, 0.0f, 2.4f);
  const Vec3 local = meterTranslationToGltfLocal(meters, kGltfCentimeterToMeterScale);
  expect_near(local.x, -812.5f, "west light local x is metres / 0.008");
  expect_near(local.y, 0.0f, "west light local y");
  expect_near(local.z, 300.0f, "west light local z");
  expect_near(local.x * kGltfCentimeterToMeterScale, meters.x,
              "parent 0.008 restores world metres x");
  expect_near(local.z * kGltfCentimeterToMeterScale, meters.z,
              "parent 0.008 restores world metres z");
  expect_true("metre courtyard looks like metre space",
              looksLikeMeterSpaceTranslation(meters));
  expect_true("converted local is not metre space",
              !looksLikeMeterSpaceTranslation(local));

  Mat4 parent_cm(1.0f);
  parent_cm[0][0] = kGltfCentimeterToMeterScale;
  parent_cm[1][1] = kGltfCentimeterToMeterScale;
  parent_cm[2][2] = kGltfCentimeterToMeterScale;
  parent_cm[3] = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
  const Vec3 world_t = Vec3(parent_cm * Vec4(local, 1.0f));
  expect_near(world_t.x, meters.x, "scaled parent keeps Unique on courtyard x");
  expect_near(world_t.z, meters.z, "scaled parent keeps Unique on courtyard z");
  const Mat4 gizmo_world =
      makeLightGizmoWorldMatrix(parent_cm * glm::translate(Mat4(1.0f), local),
                                LightGizmoKind::point);
  const Vec3 icon_origin = overlayGizmoWorldOrigin(gizmo_world);
  expect_near(icon_origin.x, meters.x, "scaled Unique world parks icon at metres x");
  expect_near(icon_origin.z, meters.z, "scaled Unique world parks icon at metres z");

  const Mat4 mesh_matched = makeLightGizmoWorldMatchingMesh(
      parent_cm * glm::translate(Mat4(1.0f), local), parent_cm,
      LightGizmoKind::point);
  const Vec3 mesh_icon = overlayGizmoWorldOrigin(mesh_matched);
  expect_near(mesh_icon.x, local.x, "icon origin undoes Unique 0.008 onto mesh x");
  expect_near(mesh_icon.z, local.z, "icon origin undoes Unique 0.008 onto mesh z");
  expect_near(glm::length(Vec3(mesh_matched[0])), 1.0f / kGltfCentimeterToMeterScale,
              "cm-space display basis matches 1/0.008");

  Mat4 identity_parent(1.0f);
  const Mat4 identity_matched = makeLightGizmoWorldMatchingMesh(
      glm::translate(Mat4(1.0f), local), identity_parent, LightGizmoKind::point);
  const Vec3 identity_icon = overlayGizmoWorldOrigin(identity_matched);
  expect_near(identity_icon.x, local.x, "identity Unique keeps centimetre icon x");
  expect_near(identity_icon.z, local.z, "identity Unique keeps centimetre icon z");

  const Vec3 collapsed = Vec3(parent_cm * Vec4(meters, 1.0f));
  expect_true("metre locals under 0.008 parent collapse toward origin",
              glm::length(collapsed) < 0.1f);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("light_gizmo_geometry_test: all passed\n");
  return 0;
}
