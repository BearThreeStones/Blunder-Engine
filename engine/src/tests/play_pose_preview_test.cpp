#include "runtime/function/editor/document_history.h"
#include "runtime/project/play_pose_preview.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"

#include "EASTL/string.h"

#include <cstdio>
#include <cmath>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool nearly(float a, float b) { return std::fabs(a - b) < 1.0e-5f; }

}  // namespace

int main() {
  using namespace Blunder;

  {
    SceneInstance scene;
    (void)scene.createEntity("Hero", Vec3(1, 0, 0), glm::identity<Quat>(),
                             Vec3(1));
    EntityId unnamed =
        scene.createEntity("tmp", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1));
    scene.getEntity(unnamed)->setName("");
    PlayIpcPosesRecord poses;
    collectPlayPoses(scene, poses);
    expect_true("one named pose", poses.entities.size() == 1);
    expect_true("named is Hero", poses.entities[0].name == "Hero");
    expect_true("unnamed omitted", poses.entities[0].name != "");
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Hero", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1));
    scene.tick(0.f);
    PlayPoseOverlayMap overlay;
    PlayPoseLocalTrs pose;
    pose.t[0] = 10.f;
    pose.t[1] = 0.f;
    pose.t[2] = 0.f;
    overlay[eastl::string("Hero")] = pose;
    const Mat4 live = scene.getWorldMatrix(id);
    const Mat4 posed =
        worldMatrixWithPlayPoseOverlay(scene, id, &overlay);
    expect_true("overlay moves x", nearly(posed[3].x, 10.f));
    expect_true("live stays authored", nearly(live[3].x, 0.f));
    expect_true("entity live position",
                scene.getEntity(id)->getPosition() == Vec3(0, 0, 0));

    DocumentHistory history;
    history.markSaveBaseline();
    (void)worldMatrixWithPlayPoseOverlay(scene, id, &overlay);
    expect_true("overlay does not dirty history",
                !history.isDirtyRelativeToSave());
    expect_true("overlay command count 0", history.commandCount() == 0);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "play_pose_preview_test: all passed\n");
  return 0;
}
