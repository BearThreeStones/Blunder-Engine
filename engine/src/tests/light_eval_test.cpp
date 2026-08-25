#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_starter.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b, float epsilon = 1e-4f) {
  return std::fabs(a.x - b.x) <= epsilon && std::fabs(a.y - b.y) <= epsilon &&
         std::fabs(a.z - b.z) <= epsilon;
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  {
    expect_true("identity emit is world -Z",
                vec3_near(lightWorldEmit(Mat4(1.0f)), Vec3(0.0f, 0.0f, -1.0f)));
    expect_true("identity shading L is world +Z",
                vec3_near(lightShadingL(LightType::directional, Vec3(0.0f, 0.0f, -1.0f),
                                        Vec3(0.0f), Vec3(0.0f)),
                          Vec3(0.0f, 0.0f, 1.0f)));
    expect_true("beyond range is 0",
                punctualRangeAttenuation(10.0f, 8.0f) == 0.0f);
    expect_true("at range is 0", punctualRangeAttenuation(8.0f, 8.0f) == 0.0f);
    expect_true("inside range positive",
                punctualRangeAttenuation(1.0f, 8.0f) > 0.0f);
  }

  {
    SceneInstance scene;
    const EntityId mesh_a =
        scene.createEntity("A", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId mesh_b =
        scene.createEntity("B", Vec3(1, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(mesh_a, MeshRendererComponent{});
    scene.setMeshRenderer(mesh_b, MeshRendererComponent{});

    LightComponent empty_link;
    empty_link.type = LightType::directional;
    const EntityId light_all = scene.createEntity(
        "All", Vec3(0, 0, 8), glm::identity<Quat>(), Vec3(1));
    scene.setLight(light_all, empty_link);

    EvaluatedLight gathered[8];
    const EntityId spawned = scene.createEntity(
        "Spawned", Vec3(2, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(spawned, MeshRendererComponent{});
    expect_true("empty linking includes new mesh",
                gatherLightsForMesh(scene, spawned, gathered, 8) == 1 &&
                    gathered[0].entity_id == light_all);

    LightComponent linked;
    linked.type = LightType::point;
    linked.linking.push_back(mesh_a);
    const EntityId light_a = scene.createEntity(
        "OnlyA", Vec3(0, 0, 4), glm::identity<Quat>(), Vec3(1));
    scene.setLight(light_a, linked);
    expect_true("non-empty linking includes A",
                gatherLightsForMesh(scene, mesh_a, gathered, 8) >= 1);
    bool a_has_only_a = false;
    bool b_has_only_a = false;
    const size_t count_a = gatherLightsForMesh(scene, mesh_a, gathered, 8);
    for (size_t i = 0; i < count_a; ++i) {
      if (gathered[i].entity_id == light_a) {
        a_has_only_a = true;
      }
    }
    const size_t count_b = gatherLightsForMesh(scene, mesh_b, gathered, 8);
    for (size_t i = 0; i < count_b; ++i) {
      if (gathered[i].entity_id == light_a) {
        b_has_only_a = true;
      }
    }
    expect_true("non-empty linking hits A", a_has_only_a);
    expect_true("non-empty linking excludes B", !b_has_only_a);
  }

  {
    SceneInstance scene;
    const EntityId mesh =
        scene.createEntity("Mesh", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(mesh, MeshRendererComponent{});
    for (int i = 0; i < 9; ++i) {
      char name[16];
      std::snprintf(name, sizeof(name), "P%d", i);
      const EntityId id = scene.createEntity(name, Vec3(0, 0, 0),
                                             glm::identity<Quat>(), Vec3(1));
      LightComponent point;
      point.type = LightType::point;
      scene.setLight(id, point);
    }
    EvaluatedLight gathered[8];
    expect_true("ninth dropped",
                gatherLightsForMesh(scene, mesh, gathered, 8) == 8);
  }

  {
    SceneInstance scene;
    LightComponent first;
    first.type = LightType::directional;
    first.contribution = LightContribution::illuminateAndShadows;
    const EntityId a = scene.createEntity("DirA", Vec3(0, 0, 8),
                                          glm::identity<Quat>(), Vec3(1));
    scene.setLight(a, first);
    LightComponent second = first;
    const EntityId b = scene.createEntity("DirB", Vec3(1, 0, 8),
                                          glm::identity<Quat>(), Vec3(1));
    scene.setLight(b, second);
    expect_true("first shadow directional wins",
                pickDirectionalShadowCaster(scene) == a);

    LightComponent disabled;
    disabled.type = LightType::directional;
    disabled.enabled = false;
    scene.setLight(a, disabled);
    expect_true("disabled ignored for shadows",
                pickDirectionalShadowCaster(scene) == b);

    EvaluatedLight gathered[8];
    const EntityId mesh =
        scene.createEntity("M", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    expect_true("disabled ignored in gather",
                gatherLightsForMesh(scene, mesh, gathered, 8) == 1 &&
                    gathered[0].entity_id == b);
  }

  {
    Scene scene;
    appendNewSceneStarterEntities(scene.getEntities());
    expect_true("new scene has two entities", scene.getEntities().size() == 2);
    expect_true("first is Main Camera",
                scene.getEntities()[0].name == "Main Camera" &&
                    scene.getEntities()[0].has_camera);
    expect_true("second is Directional Light",
                scene.getEntities()[1].name == "Directional Light" &&
                    scene.getEntities()[1].has_light &&
                    scene.getEntities()[1].light.type == LightType::directional);
    expect_true("light not on camera", !scene.getEntities()[0].has_light);
    expect_true("light above XY",
                scene.getEntities()[1].position.z > 0.0f);
    const Vec3 emit = lightWorldEmit(
        glm::mat4_cast(scene.getEntities()[1].rotation));
    expect_true("starter emit slants toward ground", emit.z < 0.0f);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("light_eval_test: all passed\n");
  Blunder::g_runtime_global_context.m_logger_system.reset();
  return 0;
}
