#include <cstdio>

#include "runtime/function/scene/align_camera_target.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_eq_entity(const char* label, Blunder::EntityId actual,
                      Blunder::EntityId expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s (got %u want %u)\n", label, actual, expected);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    SceneInstance scene;
    scene.createEntity("Empty", {}, {}, {});
    const AlignCameraTargetResult result =
        resolveAlignCameraTarget(scene, {});
    expect_true("no cameras -> !ok", !result.ok);
  }

  {
    SceneInstance scene;
    const EntityId cam_id =
        scene.createEntity("Cam", {}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_id, cam);

    const eastl::vector<EntityId> selection{cam_id};
    const AlignCameraTargetResult result =
        resolveAlignCameraTarget(scene, selection);
    expect_true("single camera selected -> ok", result.ok);
    expect_eq_entity("single camera selected id", result.entity_id, cam_id);
  }

  {
    SceneInstance scene;
    const EntityId cam_a = scene.createEntity("CamA", {}, {}, {});
    const EntityId cam_b = scene.createEntity("CamB", {}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_a, cam);
    scene.setCamera(cam_b, cam);

    const eastl::vector<EntityId> selection{cam_a, cam_b};
    const AlignCameraTargetResult result =
        resolveAlignCameraTarget(scene, selection);
    expect_true("multi-select -> !ok", !result.ok);
  }

  {
    SceneInstance scene;
    const EntityId mesh_id = scene.createEntity("Mesh", {}, {}, {});
    const EntityId cam_id = scene.createEntity("Cam", {}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_id, cam);

    const eastl::vector<EntityId> selection{mesh_id};
    const AlignCameraTargetResult result =
        resolveAlignCameraTarget(scene, selection);
    expect_true("single non-camera -> !ok", !result.ok);
    (void)cam_id;
  }

  {
    SceneInstance scene;
    const EntityId cam_low = scene.createEntity("CamLow", {}, {}, {});
    const EntityId cam_high = scene.createEntity("CamHigh", {}, {}, {});
    CameraComponent low_cam{};
    CameraComponent high_cam{};
    high_cam.is_main = true;
    scene.setCamera(cam_low, low_cam);
    scene.setCamera(cam_high, high_cam);

    const AlignCameraTargetResult result =
        resolveAlignCameraTarget(scene, {});
    expect_true("no selection main -> ok", result.ok);
    expect_eq_entity("no selection prefers main", result.entity_id, cam_high);
  }

  {
    SceneInstance scene;
    const EntityId cam_low = scene.createEntity("CamLow", {}, {}, {});
    const EntityId cam_high = scene.createEntity("CamHigh", {}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_low, cam);
    scene.setCamera(cam_high, cam);

    const AlignCameraTargetResult result =
        resolveAlignCameraTarget(scene, {});
    expect_true("no selection no main -> ok", result.ok);
    expect_eq_entity("no selection picks lowest id", result.entity_id, cam_low);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("align_camera_target_test: all passed\n");
  return 0;
}
