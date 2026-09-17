#include "runtime/core/log/log_system.h"
#include "runtime/core/math/geometry.h"
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

    LightComponent linked;
    linked.type = LightType::point;
    linked.linking.push_back(mesh_a);
    const EntityId light_a = scene.createEntity(
        "OnlyA", Vec3(0, 0, 4), glm::identity<Quat>(), Vec3(1));
    scene.setLight(light_a, linked);

    EvaluatedLight deferred[k_max_deferred_light_list];
    EvaluatedLight filtered_a[k_max_evaluated_lights_per_mesh];
    EvaluatedLight filtered_b[k_max_evaluated_lights_per_mesh];
    const size_t list_count = buildDeferredLightList(
        scene, deferred, k_max_deferred_light_list);
    expect_true("deferred list has both lights", list_count == 2);
    const size_t recv_a = evaluateLightsForReceiver(
        scene, mesh_a, deferred, list_count, filtered_a,
        k_max_evaluated_lights_per_mesh);
    const size_t recv_b = evaluateLightsForReceiver(
        scene, mesh_b, deferred, list_count, filtered_b,
        k_max_evaluated_lights_per_mesh);
    bool a_has_only_a = false;
    bool b_has_only_a = false;
    for (size_t i = 0; i < recv_a; ++i) {
      if (filtered_a[i].entity_id == light_a) {
        a_has_only_a = true;
      }
    }
    for (size_t i = 0; i < recv_b; ++i) {
      if (filtered_b[i].entity_id == light_a) {
        b_has_only_a = true;
      }
    }
    expect_true("deferred linking hits A", a_has_only_a);
    expect_true("deferred linking excludes B", !b_has_only_a);
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
    EvaluatedLight deferred[k_max_deferred_light_list];
    EvaluatedLight gathered[k_max_evaluated_lights_per_mesh];
    EvaluatedLight filtered[k_max_evaluated_lights_per_mesh];
    const size_t list_count = buildDeferredLightList(
        scene, deferred, k_max_deferred_light_list);
    expect_true("nine empty-linking lights all fit in deferred list",
                list_count == 9);
    expect_true(
        "deferred ninth dropped per receiver",
        evaluateLightsForReceiver(scene, mesh, deferred, list_count, filtered,
                                  k_max_evaluated_lights_per_mesh) == 8);
    expect_true("gather ninth dropped",
                gatherLightsForMesh(scene, mesh, gathered,
                                     k_max_evaluated_lights_per_mesh) == 8);
    bool match = true;
    for (size_t i = 0; i < 8; ++i) {
      if (filtered[i].entity_id != gathered[i].entity_id) {
        match = false;
      }
    }
    expect_true("nine-light deferred receiver matches gather", match);
  }

  {
    SceneInstance scene;
    const EntityId mesh =
        scene.createEntity("Mesh", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(mesh, MeshRendererComponent{});
    EntityId ids[33];
    for (int i = 0; i < 33; ++i) {
      char name[16];
      std::snprintf(name, sizeof(name), "L%d", i);
      ids[i] = scene.createEntity(name, Vec3(0, 0, 0), glm::identity<Quat>(),
                                 Vec3(1));
      LightComponent point;
      point.type = LightType::point;
      scene.setLight(ids[i], point);
    }
    EvaluatedLight deferred[k_max_deferred_light_list];
    EvaluatedLight gathered[k_max_evaluated_lights_per_mesh];
    EvaluatedLight filtered[k_max_evaluated_lights_per_mesh];
    const size_t list_count = buildDeferredLightList(
        scene, deferred, k_max_deferred_light_list);
    expect_true("deferred list caps at 32",
                list_count == k_max_deferred_light_list);
    bool has_33rd = false;
    for (size_t i = 0; i < list_count; ++i) {
      if (deferred[i].entity_id == ids[32]) {
        has_33rd = true;
      }
    }
    expect_true("thirty-third light not in deferred list", !has_33rd);

    const size_t recv = evaluateLightsForReceiver(
        scene, mesh, deferred, list_count, filtered,
        k_max_evaluated_lights_per_mesh);
    const size_t gathered_count = gatherLightsForMesh(
        scene, mesh, gathered, k_max_evaluated_lights_per_mesh);
    expect_true("32-list receiver still 8", recv == 8 && gathered_count == 8);
    bool match = recv == gathered_count;
    for (size_t i = 0; i < recv && match; ++i) {
      if (filtered[i].entity_id != gathered[i].entity_id) {
        match = false;
      }
    }
    expect_true("32-list then per-receiver 8 matches gather", match);
  }

  {
    SceneInstance scene;
    for (int i = 0; i < 32; ++i) {
      char name[16];
      std::snprintf(name, sizeof(name), "P%d", i);
      const EntityId id = scene.createEntity(name, Vec3(0, 0, 0),
                                             glm::identity<Quat>(), Vec3(1));
      LightComponent point;
      point.type = LightType::point;
      scene.setLight(id, point);
    }
    LightComponent sun;
    sun.type = LightType::directional;
    const EntityId sun_id = scene.createEntity(
        "Sun", Vec3(0, 8, 0), glm::identity<Quat>(), Vec3(1));
    scene.setLight(sun_id, sun);
    LightComponent area;
    area.type = LightType::area;
    const EntityId area_id = scene.createEntity(
        "Area", Vec3(1, 8, 0), glm::identity<Quat>(), Vec3(1));
    scene.setLight(area_id, area);

    EvaluatedLight mixed[k_max_deferred_light_list];
    const size_t mixed_count =
        buildDeferredLightList(scene, mixed, k_max_deferred_light_list);
    bool mixed_has_sun = false;
    bool mixed_has_area = false;
    for (size_t i = 0; i < mixed_count; ++i) {
      mixed_has_sun = mixed_has_sun || mixed[i].entity_id == sun_id;
      mixed_has_area = mixed_has_area || mixed[i].entity_id == area_id;
    }
    expect_true("mixed 32-list drops later directional", !mixed_has_sun);
    expect_true("mixed 32-list drops later area", !mixed_has_area);

    EvaluatedLight fullscreen[k_max_deferred_light_list];
    const size_t fullscreen_count = buildDeferredFullscreenLightList(
        scene, fullscreen, k_max_deferred_light_list);
    expect_true("fullscreen list is sun then area",
                fullscreen_count == 2 && fullscreen[0].entity_id == sun_id &&
                    fullscreen[1].entity_id == area_id &&
                    fullscreen[0].type == LightType::directional &&
                    fullscreen[1].type == LightType::area);
  }

  {
    SceneInstance scene;
    const EntityId dummy = scene.createEntity(
        "Dummy", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(dummy, MeshRendererComponent{});
    EntityId ids[32];
    for (int i = 0; i < 32; ++i) {
      char name[16];
      std::snprintf(name, sizeof(name), "L%d", i);
      ids[i] = scene.createEntity(name, Vec3(0, 0, 0), glm::identity<Quat>(),
                                 Vec3(1));
      LightComponent point;
      point.type = LightType::point;
      point.linking.push_back(dummy);
      scene.setLight(ids[i], point);
    }
    const EntityId mesh =
        scene.createEntity("Mesh", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(mesh, MeshRendererComponent{});
    const EntityId extra = scene.createEntity(
        "L32", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    LightComponent last;
    last.type = LightType::point;
    scene.setLight(extra, last);

    EvaluatedLight deferred[k_max_deferred_light_list];
    EvaluatedLight filtered[k_max_evaluated_lights_per_mesh];
    EvaluatedLight gathered[k_max_evaluated_lights_per_mesh];
    const size_t list_count =
        buildDeferredLightList(scene, deferred, k_max_deferred_light_list);
    expect_true("deferred list still 32 with linked dummy lights",
                list_count == k_max_deferred_light_list);
    bool has_extra = false;
    for (size_t i = 0; i < list_count; ++i) {
      if (deferred[i].entity_id == extra) {
        has_extra = true;
      }
    }
    expect_true("empty-linking 33rd not in deferred list", !has_extra);
    expect_true(
        "receiver cannot revive dropped 33rd",
        evaluateLightsForReceiver(scene, mesh, deferred, list_count, filtered,
                                  k_max_evaluated_lights_per_mesh) == 0);
    expect_true("forward still sees the 33rd",
                gatherLightsForMesh(scene, mesh, gathered,
                                     k_max_evaluated_lights_per_mesh) == 1 &&
                    gathered[0].entity_id == extra);
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
    EvaluatedLight deferred[k_max_deferred_light_list];
    const size_t list_count =
        buildDeferredLightList(scene, deferred, k_max_deferred_light_list);
    expect_true("disabled ignored in deferred list",
                list_count == 1 && deferred[0].entity_id == b);
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
    scene.setObjectActive(a, false);
    expect_true("inactive ignored for shadows",
                pickDirectionalShadowCaster(scene) == b);

    EvaluatedLight gathered[8];
    const EntityId mesh =
        scene.createEntity("M", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(mesh, MeshRendererComponent{});
    expect_true("inactive ignored in gather",
                gatherLightsForMesh(scene, mesh, gathered, 8) == 1 &&
                    gathered[0].entity_id == b);

    const EntityId parent = scene.createEntity(
        "P", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId child = scene.createEntity(
        "ChildLight", Vec3(0, 0, 4), glm::identity<Quat>(), Vec3(1), parent);
    LightComponent child_light;
    child_light.type = LightType::point;
    scene.setLight(child, child_light);
    scene.setObjectActive(parent, false);
    expect_true("child light stays locally on", scene.isObjectActive(child));
    expect_true("inactive parent skips child light",
                gatherLightsForMesh(scene, mesh, gathered, 8) == 1 &&
                    gathered[0].entity_id == b);
    EvaluatedLight deferred[k_max_deferred_light_list];
    const size_t list_count =
        buildDeferredLightList(scene, deferred, k_max_deferred_light_list);
    expect_true("inactive parent skips child in deferred list",
                list_count == 1 && deferred[0].entity_id == b);
  }

  {
    SceneInstance scene;
    const EntityId mesh =
        scene.createEntity("Mesh", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(mesh, MeshRendererComponent{});
    LightComponent illum;
    illum.type = LightType::point;
    const EntityId a = scene.createEntity("P", Vec3(0, 0, 0),
                                         glm::identity<Quat>(), Vec3(1));
    scene.setLight(a, illum);
    LightComponent shadows;
    shadows.type = LightType::point;
    shadows.contribution = LightContribution::shadowsOnly;
    const EntityId b = scene.createEntity("S", Vec3(1, 0, 0),
                                         glm::identity<Quat>(), Vec3(1));
    scene.setLight(b, shadows);
    EvaluatedLight deferred[k_max_deferred_light_list];
    const size_t list_count =
        buildDeferredLightList(scene, deferred, k_max_deferred_light_list);
    expect_true("shadowsOnly point stays on deferred list",
                list_count == 2);
    bool has_shadows_only = false;
    for (size_t i = 0; i < list_count; ++i) {
      if (deferred[i].entity_id == b) {
        has_shadows_only = true;
      }
    }
    expect_true("shadowsOnly point is evaluated", has_shadows_only);
    expect_true("shadowsOnly point is not a no-op",
                !lightContributionIsNoOpThisSlice(shadows));
  }

  {
    LightComponent area;
    area.type = LightType::area;
    area.contribution = LightContribution::shadowsOnly;
    expect_true("shadowsOnly area is a no-op this slice",
                lightContributionIsNoOpThisSlice(area));
    LightComponent spot;
    spot.type = LightType::spot;
    spot.contribution = LightContribution::shadowsOnly;
    expect_true("shadowsOnly spot is evaluated",
                !lightContributionIsNoOpThisSlice(spot));
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

  {
    // Punctual range and the 1/d^2 term are metre-authored but shaded against
    // world distances. Centimetre Sponza put 6-18 m ranges against 200-1000 unit
    // distances, so every courtyard light attenuated to exactly zero.
    SceneInstance scene;
    const EntityId point = scene.createEntity(
        "Courtyard Point West", Vec3(-812.5f, 0.0f, 300.0f), glm::identity<Quat>(),
        Vec3(1));
    LightComponent point_light{};
    point_light.type = LightType::point;
    point_light.enabled = true;
    point_light.contribution = LightContribution::illuminateOnly;
    point_light.range = 12.0f;
    point_light.intensity = 16.0f;
    point_light.color = Vec3(1.0f, 0.12f, 0.58f);
    scene.setLight(point, point_light);

    const EntityId sun = scene.createEntity(
        "Directional Light", Vec3(0.0f, 0.0f, 1000.0f), glm::identity<Quat>(), Vec3(1));
    LightComponent sun_light{};
    sun_light.type = LightType::directional;
    sun_light.enabled = true;
    sun_light.intensity = 1.6f;
    sun_light.color = Vec3(0.42f, 0.52f, 0.78f);
    scene.setLight(sun, sun_light);

    scene.setWorldBounds(
        AABB{Vec3(-1920.0f, -1105.0f, -126.0f), Vec3(1799.0f, 1182.0f, 1429.0f)});
    scene.tick(0.0f);
    expect_true("Sponza bounds read as a centimetre world",
                sceneIsCentimetreWorld(scene));
    expect_true("Sponza is 125 world units per metre",
                std::fabs(sceneWorldUnitsPerMetre(scene) - 125.0f) < 1e-3f);

    EvaluatedLight lights[k_max_deferred_light_list];
    const size_t count =
        buildDeferredLightList(scene, lights, k_max_deferred_light_list);
    expect_true("both lights listed", count == 2);
    const EvaluatedLight* evaluated_point = nullptr;
    const EvaluatedLight* evaluated_sun = nullptr;
    for (size_t i = 0; i < count; ++i) {
      if (lights[i].type == LightType::point) {
        evaluated_point = &lights[i];
      } else if (lights[i].type == LightType::directional) {
        evaluated_sun = &lights[i];
      }
    }
    expect_true("point range converts to world units",
                evaluated_point != nullptr &&
                    std::fabs(evaluated_point->range - 1500.0f) < 1e-2f);
    // A 3 m surface at 375 world units used to fall outside a range of 12.
    expect_true("courtyard surface is now inside range",
                evaluated_point != nullptr &&
                    punctualRangeAttenuation(375.0f, evaluated_point->range) > 0.0f);
    // Intensity carries the square so 1/d_world^2 still equals 1/d_metre^2.
    expect_true("point radiance is unit-invariant",
                evaluated_point != nullptr &&
                    std::fabs(evaluated_point->color_times_intensity.x *
                                  punctualRangeAttenuation(375.0f,
                                                           evaluated_point->range) -
                              point_light.color.x * point_light.intensity *
                                  punctualRangeAttenuation(3.0f, 12.0f)) < 1e-3f);
    expect_true("directional keeps authored radiance",
                evaluated_sun != nullptr &&
                    std::fabs(evaluated_sun->color_times_intensity.x -
                              sun_light.color.x * sun_light.intensity) < 1e-4f);
    expect_true("world positions are never rescaled",
                evaluated_point != nullptr &&
                    std::fabs(evaluated_point->world_position.x + 812.5f) < 1e-3f);
  }

  {
    SceneInstance scene;
    const EntityId point = scene.createEntity(
        "Point", Vec3(2.0f, 0.0f, 1.0f), glm::identity<Quat>(), Vec3(1));
    LightComponent point_light{};
    point_light.type = LightType::point;
    point_light.enabled = true;
    point_light.range = 12.0f;
    point_light.intensity = 16.0f;
    scene.setLight(point, point_light);
    scene.setWorldBounds(AABB{Vec3(-8.0f, -8.0f, 0.0f), Vec3(8.0f, 8.0f, 6.0f)});
    scene.tick(0.0f);
    EvaluatedLight lights[k_max_deferred_light_list];
    const size_t count =
        buildDeferredLightList(scene, lights, k_max_deferred_light_list);
    expect_true("metre world leaves range authored",
                count == 1 && std::fabs(lights[0].range - 12.0f) < 1e-4f);
    expect_true("metre world leaves intensity authored",
                count == 1 &&
                    std::fabs(lights[0].color_times_intensity.x - 16.0f) < 1e-4f);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("light_eval_test: all passed\n");
  Blunder::g_runtime_global_context.m_logger_system.reset();
  return 0;
}
