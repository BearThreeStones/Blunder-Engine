#include "runtime/function/render/overlay/camera_gizmo_geometry.h"
#include "runtime/function/render/overlay/collision_gizmo_geometry.h"
#include "runtime/function/render/overlay/gizmo_line_command.h"
#include "runtime/function/render/overlay/light_gizmo_geometry.h"
#include "runtime/function/render/gpu_driven/gpu_driven_cull.h"

#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  GizmoLineCommandStream stream;
  const glm::vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
  for (int i = 0; i < 3000; ++i) {
    const float x = static_cast<float>(i);
    stream.push(GizmoLineDrawStyle::line, glm::vec3(x, 0.0f, 0.0f),
                glm::vec3(x + 1.0f, 0.0f, 0.0f), glm::vec3(0.0f), red);
  }
  expect_true("3000 segments stay in the command stream", stream.size() == 3000u);
  expect_true("batched path is one draw, not per-segment",
              gizmoLineBatchedDrawCalls(stream.size()) == 1u);
  expect_true("empty stream issues no draws",
              gizmoLineBatchedDrawCalls(0u) == 0u);

  const uint64_t hash_a = stream.hash();
  GizmoLineCommandStream same;
  for (int i = 0; i < 3000; ++i) {
    const float x = static_cast<float>(i);
    same.push(GizmoLineDrawStyle::line, glm::vec3(x, 0.0f, 0.0f),
              glm::vec3(x + 1.0f, 0.0f, 0.0f), glm::vec3(0.0f), red);
  }
  expect_true("identical commands share redraw hash", same.hash() == hash_a);

  GizmoLineCommandStream changed;
  changed.push(GizmoLineDrawStyle::line, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
               glm::vec3(0.0f), red);
  expect_true("different commands change redraw hash", changed.hash() != hash_a);

  uint64_t uploaded = 0;
  auto skip_upload = [&](uint64_t hash) {
    if (uploaded == hash) {
      return true;
    }
    uploaded = hash;
    return false;
  };
  expect_true("first submit uploads", !skip_upload(hash_a));
  expect_true("same hash skips GPU upload", skip_upload(hash_a));
  expect_true("new hash uploads again", !skip_upload(changed.hash()));

  glm::mat4 view = glm::lookAt(glm::vec3(0.0f, -8.0f, 0.0f), glm::vec3(0.0f),
                               glm::vec3(0.0f, 0.0f, 1.0f));
  glm::mat4 projection =
      glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  glm::vec4 planes[6];
  extractFrustumPlanes(projection * view, planes);
  expect_true("origin Unique stays visible",
              gizmoSphereVisible(glm::vec3(0.0f), 0.5f, planes));
  expect_true("Unique behind camera is culled",
              !gizmoSphereVisible(glm::vec3(0.0f, -80.0f, 0.0f), 0.5f, planes));

  LightGizmoShape point{};
  point.kind = LightGizmoKind::point;
  point.range = 50.0f;
  expect_true("unselected point cull radius is display size",
              lightGizmoLocalCullRadius(point) < 2.0f);
  point.show_range = true;
  expect_true("selected point cull radius covers range",
              lightGizmoLocalCullRadius(point) > 49.0f);

  CameraGizmoFrame frame =
      buildCameraGizmoFrameLocal(glm::radians(60.0f), 1.0f, 1.0f);
  expect_true("camera frame cull radius covers corners",
              cameraGizmoLocalCullRadius(frame) > 0.5f);

  ColliderComponent box{};
  box.shape = ColliderShapeKind::Box;
  box.box_half_extents = Vec3(2.0f, 0.5f, 0.5f);
  float box_r = 0.0f;
  expect_true("box collider has cull radius",
              colliderGizmoTryLocalCullRadius(box, box_r) && box_r > 1.9f);

  glm::vec4 clip_pos(0.0f, 0.0f, 0.5f, 1.0f);
  glm::vec4 clip_dir(1.0f, 0.0f, 0.5f, 1.0f);
  const glm::vec2 n =
      gizmoClipLineScreenNormal(clip_pos, clip_dir, 1920.0f, 1080.0f);
  expect_true("horizontal clip line has vertical screen normal",
              std::fabs(n.x) < 0.05f && n.y > 0.9f);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "gizmo_line_batch_test: all passed\n");
  return 0;
}
