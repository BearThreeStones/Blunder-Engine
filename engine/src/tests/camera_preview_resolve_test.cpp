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

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("camera_preview_resolve_test: all passed\n");
  return 0;
}
