#include <cmath>
#include <cstdio>

#include "runtime/function/scene/play_camera_resolve.h"
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

void expect_near(const char* label, float actual, float expected) {
  if (std::fabs(actual - expected) > 1e-4f) {
    std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, actual, expected);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const ResolvedPlayCamera result = resolvePlayCamera(nullptr, 0, 16.0f / 9.0f);
    expect_true("empty cameras -> !ok", !result.ok);
  }

  {
    PlayCameraResolveInput input{};
    input.entity_id = 42;
    input.world = Mat4(1.0f);
    input.world[3] = Vec4(1.0f, 2.0f, 3.0f, 1.0f);
    const ResolvedPlayCamera result = resolvePlayCamera(&input, 1, 16.0f / 9.0f);
    expect_true("one camera -> ok", result.ok);
    expect_eq_entity("one camera entity id", result.entity_id, 42);
  }

  {
    PlayCameraResolveInput cameras[2]{};
    cameras[0].entity_id = 1;
    cameras[1].entity_id = 2;
    cameras[1].camera.is_main = true;
    const ResolvedPlayCamera result = resolvePlayCamera(cameras, 2, 16.0f / 9.0f);
    expect_true("main camera -> ok", result.ok);
    expect_eq_entity("prefers is_main camera", result.entity_id, 2);
  }

  {
    PlayCameraResolveInput cameras[2]{};
    cameras[0].entity_id = 10;
    cameras[1].entity_id = 11;
    const ResolvedPlayCamera result = resolvePlayCamera(cameras, 2, 16.0f / 9.0f);
    expect_true("no main -> ok", result.ok);
    expect_eq_entity("no main picks first", result.entity_id, 10);
  }

  {
    PlayCameraResolveInput input{};
    input.entity_id = 7;
    input.camera.vertical_fov_degrees = 60.0f;
    input.camera.near_clip = 0.5f;
    input.camera.far_clip = 500.0f;
    const ResolvedPlayCamera result = resolvePlayCamera(&input, 1, 2.0f);
    expect_true("clip/fov -> ok", result.ok);
    expect_near("vertical fov radians", result.vertical_fov_radians,
                glm::radians(60.0f));
    expect_near("near clip", result.near_clip, 0.5f);
    expect_near("far clip", result.far_clip, 500.0f);
  }

  {
    SceneInstance scene;
    const EntityId no_cam = scene.createEntity("Empty", {}, {}, {});
    const EntityId cam_low = scene.createEntity("CamLow", {}, {}, {});
    const EntityId cam_high = scene.createEntity("CamHigh", {}, {}, {});
    (void)no_cam;

    CameraComponent low_cam{};
    CameraComponent high_cam{};
    high_cam.is_main = true;
    scene.setCamera(cam_high, high_cam);
    scene.setCamera(cam_low, low_cam);

    const ResolvedPlayCamera main_result =
        resolvePlayCameraFromScene(scene, 16.0f / 9.0f);
    expect_true("scene main -> ok", main_result.ok);
    expect_eq_entity("scene prefers is_main", main_result.entity_id, cam_high);
  }

  {
    SceneInstance scene;
    scene.createEntity("Empty", {}, {}, {});
    const EntityId cam_low = scene.createEntity("CamLow", {}, {}, {});
    const EntityId cam_high = scene.createEntity("CamHigh", {}, {}, {});

    CameraComponent cam{};
    scene.setCamera(cam_high, cam);
    scene.setCamera(cam_low, cam);

    const ResolvedPlayCamera first_result =
        resolvePlayCameraFromScene(scene, 16.0f / 9.0f);
    expect_true("scene no main -> ok", first_result.ok);
    expect_eq_entity("scene no main picks lowest entity id", first_result.entity_id,
                     cam_low);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("play_camera_resolve_test: all passed\n");
  return 0;
}
