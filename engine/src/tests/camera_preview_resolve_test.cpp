#include <cmath>
#include <cstdio>

#include "runtime/function/render/overlay/camera_preview_resolve.h"
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
    SceneInstance scene;
    const eastl::vector<EntityId> selection{};
    const CameraPreviewTargetResult result =
        resolveCameraPreviewTarget(scene, k_invalid_entity_id, selection);
    expect_true("empty selection -> !ok", !result.ok);
  }

  {
    SceneInstance scene;
    const EntityId cam_id = scene.createEntity("Cam", {}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_id, cam);

    const eastl::vector<EntityId> selection{cam_id};
    const CameraPreviewTargetResult result =
        resolveCameraPreviewTarget(scene, cam_id, selection);
    expect_true("primary has camera -> ok", result.ok);
    expect_eq_entity("primary has camera id", result.entity_id, cam_id);
  }

  {
    SceneInstance scene;
    const EntityId mesh_id = scene.createEntity("Mesh", {}, {}, {});
    const EntityId cam_id = scene.createEntity("Cam", {}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_id, cam);

    const eastl::vector<EntityId> selection{mesh_id, cam_id};
    const CameraPreviewTargetResult result =
        resolveCameraPreviewTarget(scene, mesh_id, selection);
    expect_true("primary mesh second camera -> ok", result.ok);
    expect_eq_entity("primary mesh second camera id", result.entity_id, cam_id);
  }

  {
    SceneInstance scene;
    const EntityId mesh_a = scene.createEntity("MeshA", {}, {}, {});
    const EntityId mesh_b = scene.createEntity("MeshB", {}, {}, {});

    const eastl::vector<EntityId> selection{mesh_a, mesh_b};
    const CameraPreviewTargetResult result =
        resolveCameraPreviewTarget(scene, mesh_a, selection);
    expect_true("no cameras in selection -> !ok", !result.ok);
  }

  {
    SceneInstance scene;
    const EntityId cam_id = scene.createEntity("Cam", {}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_id, cam);
    scene.softDeleteEntity(cam_id);

    const eastl::vector<EntityId> selection{cam_id};
    const CameraPreviewTargetResult result =
        resolveCameraPreviewTarget(scene, cam_id, selection);
    expect_true("tombstoned camera skipped -> !ok", !result.ok);
  }

  {
    SceneInstance scene;
    const EntityId mesh_id = scene.createEntity("Mesh", {}, {}, {});
    const ResolvedPlayCamera result =
        buildCameraPreviewMatrices(scene, mesh_id, 16.0f / 9.0f);
    expect_true("no camera component -> !ok", !result.ok);
  }

  {
    SceneInstance scene;
    const EntityId cam_id =
        scene.createEntity("Cam", {1.0f, 2.0f, 3.0f}, {}, {});
    CameraComponent cam{};
    cam.vertical_fov_degrees = 60.0f;
    cam.near_clip = 0.5f;
    cam.far_clip = 500.0f;
    scene.setCamera(cam_id, cam);
    scene.tick(0.0f);

    constexpr float aspect = 2.0f;
    const ResolvedPlayCamera result =
        buildCameraPreviewMatrices(scene, cam_id, aspect);
    expect_true("camera matrices -> ok", result.ok);
    expect_eq_entity("camera matrices entity id", result.entity_id, cam_id);
    expect_near("camera position x", result.position.x, 1.0f);
    expect_near("camera position y", result.position.y, 2.0f);
    expect_near("camera position z", result.position.z, 3.0f);
    expect_near("vertical fov radians", result.vertical_fov_radians,
                glm::radians(60.0f));
    expect_near("near clip", result.near_clip, 0.5f);
    expect_near("far clip", result.far_clip, 500.0f);
    expect_true("projection y flip", result.projection[1][1] < 0.0f);
    Mat4 expected_proj =
        glm::perspectiveZO(glm::radians(60.0f), aspect, 0.5f, 500.0f);
    expected_proj[1][1] *= -1.0f;
    expect_near("projection y scale", result.projection[1][1],
                expected_proj[1][1]);
    expect_near("projection uses perspectiveZO", result.projection[2][2],
                expected_proj[2][2]);
  }

  {
    SceneInstance scene;
    const EntityId cam_id =
        scene.createEntity("LookDown", {0.0f, 0.0f, 2.0f}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_id, cam);
    scene.tick(0.0f);
    const ResolvedPlayCamera result =
        buildCameraPreviewMatrices(scene, cam_id, 16.0f / 9.0f);
    expect_true("preview look-down -> ok", result.ok);
    expect_near("preview look-down forward z", result.forward.z, -1.0f);
    const Vec4 origin_clip =
        result.projection * result.view * Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const Vec3 origin_ndc = Vec3(origin_clip) / origin_clip.w;
    expect_true("preview origin vulkan depth",
                origin_ndc.z > 0.0f && origin_ndc.z < 1.0f);
  }

  {
    SceneInstance scene;
    const EntityId cam_id = scene.createEntity("Cam", {}, {}, {});
    CameraComponent cam{};
    scene.setCamera(cam_id, cam);
    scene.softDeleteEntity(cam_id);

    const ResolvedPlayCamera result =
        buildCameraPreviewMatrices(scene, cam_id, 16.0f / 9.0f);
    expect_true("tombstoned camera matrices -> !ok", !result.ok);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("camera_preview_resolve_test: all passed\n");
  return 0;
}
