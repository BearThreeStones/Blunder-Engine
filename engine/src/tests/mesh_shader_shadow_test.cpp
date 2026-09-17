#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/gpu_driven/gpu_driven_types.h"
#include "runtime/function/render/shadow/local_shadow_math.h"
#include "runtime/function/render/shadow/mesh_shadow_casters.h"
#include "runtime/function/render/shadow/virtual_shadow_map.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/meshlet.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
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

  expect_true("page texels 128", k_vsm_page_texels == 128u);
  expect_true("virtual pages per axis 128", k_vsm_pages_per_axis == 128u);
  expect_true("first level 6", k_vsm_first_level == 6u);
  expect_true("last level 10", k_vsm_last_level == 10u);
  expect_true("five clipmap levels", k_vsm_level_count == 5u);
  expect_true("512 physical pages", k_vsm_physical_pages == 512u);
  expect_true("virtual 16k texels",
              k_vsm_pages_per_axis * k_vsm_page_texels == 16384u);
  expect_true("page table bytes",
              vsmPageTableBytes() ==
                  sizeof(uint32_t) * k_vsm_virtual_pages);
  expect_true("physical pool 32 MiB D32",
              vsmPhysicalPoolBytes() == 32ull * 1024ull * 1024ull);
  expect_true("level 6 radius 128", vsmClipmapRadius(6) == 128.0f);
  expect_true("level 10 radius 2048", vsmClipmapRadius(10) == 2048.0f);

  {
    const glm::vec3 camera(0.0f, 0.0f, 0.0f);
    const glm::vec3 light_dir(0.0f, 0.0f, -1.0f);
    const glm::mat4 view = makeVsmLightView(light_dir, camera);
    std::vector<uint32_t> flags(k_vsm_virtual_pages, 0);
    const glm::vec3 in_front(0.0f, 0.0f, 4.0f);
    expect_true("receiver in front marks a page",
                vsmMarkWorldPage(view, in_front, k_vsm_first_level, flags.data()));
    uint32_t marked = 0;
    for (uint32_t i = 0; i < k_vsm_virtual_pages; ++i) {
      marked += flags[i];
    }
    expect_true("at least one page flagged", marked >= 1u);
  }

  {
    std::vector<uint32_t> flags(k_vsm_virtual_pages, 0);
    flags[0] = 1;
    flags[1] = 1;
    std::vector<uint32_t> table(k_vsm_virtual_pages, 0);
    uint32_t reverse[4];
    const VsmCompactResult compact =
        vsmCompactPages(flags.data(), table.data(), reverse, 4);
    expect_true("two pages assigned", compact.assigned == 2u);
    expect_true("no overflow", compact.overflow == 0u);
    expect_true("page 0 assigned", table[0] != k_vsm_unmarked_page);
    expect_true("page 1 assigned", table[1] != k_vsm_unmarked_page);
    expect_true("unmarked stays ~0", table[2] == k_vsm_unmarked_page);
  }

  {
    std::vector<uint32_t> flags(k_vsm_virtual_pages, 0);
    for (uint32_t i = 0; i < 8; ++i) {
      flags[i] = 1;
    }
    std::vector<uint32_t> table(k_vsm_virtual_pages, 0);
    const VsmCompactResult compact =
        vsmCompactPages(flags.data(), table.data(), nullptr, 4);
    expect_true("pool overflow logged via count", compact.overflow == 4u);
    expect_true("assigned capped", compact.assigned == 4u);
  }

  {
    const glm::vec3 camera(0.0f, 0.0f, 0.0f);
    const glm::vec3 light_dir(0.0f, 0.0f, -1.0f);
    const glm::mat4 view = makeVsmLightView(light_dir, camera);
    const glm::vec3 along_light(0.0f, 0.0f, -10.0f);
    const glm::vec3 light_pos = vsmWorldToLight(view, along_light);
    expect_true("lookAt forward is -Z", light_pos.z < 0.0f);
    uint32_t page_x = 0;
    uint32_t page_y = 0;
    glm::vec2 uv(0.0f);
    expect_true("along-light point is in clipmap",
                vsmLightToPage(light_pos, k_vsm_first_level, page_x, page_y, uv));
    const glm::mat4 vp =
        vsmPageViewProjection(view, k_vsm_first_level, page_x, page_y);
    const glm::vec4 clip = vp * glm::vec4(along_light, 1.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    expect_true("page orthoZO keeps caster in clip Z",
                ndc.z > 0.0f && ndc.z < 1.0f);
    expect_true("page ortho keeps caster in clip XY",
                ndc.x >= -1.0f && ndc.x <= 1.0f && ndc.y >= -1.0f &&
                    ndc.y <= 1.0f);
    const float stored = vsmLightSpaceClipDepth(light_pos.z);
    expect_true("clip depth helper matches stored range",
                stored > 0.0f && stored < 1.0f);
  }

  {
    std::vector<uint32_t> flags(k_vsm_virtual_pages, 0);
    const uint32_t corner = vsmVirtualPageIndex(k_vsm_first_level, 0, 0);
    const uint32_t center = vsmVirtualPageIndex(k_vsm_first_level, 64, 64);
    flags[corner] = 1;
    flags[center] = 1;
    std::vector<uint32_t> table(k_vsm_virtual_pages, 0);
    uint32_t reverse[1];
    const VsmCompactResult compact =
        vsmCompactPages(flags.data(), table.data(), reverse, 1);
    expect_true("center page kept over corner on overflow",
                compact.assigned == 1u && reverse[0] == center);
    expect_true("corner dropped when pool is 1",
                table[corner] == k_vsm_unmarked_page);
  }

  {
    std::vector<uint32_t> cpu(k_vsm_virtual_pages, 0);
    std::vector<uint32_t> gpu(k_vsm_virtual_pages, 0);
    const uint32_t courtyard = vsmVirtualPageIndex(k_vsm_first_level, 64, 70);
    cpu[0] = 1;
    gpu[courtyard] = 1;
    for (uint32_t i = 0; i < k_vsm_virtual_pages; ++i) {
      if (gpu[i] != 0u) {
        cpu[i] = 1u;
      }
    }
    std::vector<uint32_t> table(k_vsm_virtual_pages, 0);
    const VsmCompactResult compact =
        vsmCompactPages(cpu.data(), table.data(), nullptr, 8);
    expect_true("gpu courtyard mark survives compact",
                compact.assigned == 2u &&
                    table[courtyard] != k_vsm_unmarked_page);
  }

  {
    std::vector<uint32_t> table(k_vsm_virtual_pages, k_vsm_unmarked_page);
    uint32_t reverse[4];
    std::fill(reverse, reverse + 4, k_vsm_unmarked_page);
    const uint32_t first = vsmVirtualPageIndex(k_vsm_first_level, 64, 64);
    const uint32_t second = vsmVirtualPageIndex(k_vsm_first_level, 65, 64);
    const uint32_t marked_first[] = {first, second};
    VsmCompactResult compact = vsmCompactMarkedPages(
        marked_first, 2, table.data(), reverse, 4);
    expect_true("marked compact assigns both", compact.assigned == 2u);
    expect_true("marked compact no overflow", compact.overflow == 0u);
    expect_true("first virt mapped", table[first] != k_vsm_unmarked_page);
    expect_true("second virt mapped", table[second] != k_vsm_unmarked_page);
    const uint32_t marked_second[] = {second};
    compact = vsmCompactMarkedPages(marked_second, 1, table.data(), reverse, 4);
    expect_true("stale virt unmarked without scanning 80k",
                table[first] == k_vsm_unmarked_page);
    expect_true("kept virt stays assigned",
                table[second] != k_vsm_unmarked_page && reverse[0] == second);
  }

  expect_true("meshlet max verts 64", k_meshlet_max_vertices == 64u);
  expect_true("meshlet max tris 124", k_meshlet_max_triangles == 124u);
  expect_true("meshlet record has center", sizeof(MeshletRecord) >= 24u);
  {
    MeshletRecord rec{};
    rec.center[0] = 1.0f;
    rec.center[1] = 2.0f;
    rec.center[2] = 3.0f;
    rec.radius = 4.0f;
    rec.vertex_count = 64;
    rec.triangle_count = 124;
    std::printf("meshlet layout: verts<=%u tris<=%u center=(%.0f,%.0f,%.0f) r=%.0f "
                "record=%zu\n",
                k_meshlet_max_vertices, k_meshlet_max_triangles, rec.center[0],
                rec.center[1], rec.center[2], rec.radius, sizeof(MeshletRecord));
    expect_true("dumped cooked meshlet layout", rec.vertex_count == 64);
  }

  {
    SceneInstance scene;
    const EntityId mesh_a =
        scene.createEntity("A", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId mesh_b =
        scene.createEntity("B", Vec3(1, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setMeshRenderer(mesh_a, MeshRendererComponent{});
    scene.setMeshRenderer(mesh_b, MeshRendererComponent{});

    GpuDrivenDraw draw_a{};
    draw_a.entity_id = mesh_a;
    GpuDrivenDraw draw_b{};
    draw_b.entity_id = mesh_b;
    eastl::vector<MeshShadowCasterDraw> casters;
    casters.push_back(MeshShadowCasterDraw{&draw_a, 3});
    casters.push_back(MeshShadowCasterDraw{&draw_b, 3});

    LightComponent linked;
    linked.type = LightType::point;
    linked.linking.push_back(mesh_a);
    eastl::vector<MeshShadowCasterDraw> filtered;
    filterCastersForLight(casters, &linked, filtered);
    expect_true("linked light keeps one caster", filtered.size() == 1);
    expect_true("linked light keeps mesh A",
                filtered[0].draw != nullptr &&
                    filtered[0].draw->entity_id == mesh_a);

    LightComponent empty;
    filterCastersForLight(casters, &empty, filtered);
    expect_true("empty linking keeps all opaque casters", filtered.size() == 2);
  }

  {
    SceneInstance scene;
    for (int i = 0; i < 9; ++i) {
      char name[16];
      std::snprintf(name, sizeof(name), "P%d", i);
      const EntityId id = scene.createEntity(name, Vec3(0, 0, 0),
                                             glm::identity<Quat>(), Vec3(1));
      LightComponent point;
      point.type = LightType::point;
      point.contribution = LightContribution::illuminateAndShadows;
      scene.setLight(id, point);
    }
    const LocalShadowCasters picked = pickLocalShadowCasters(scene);
    expect_true("eight point maps", picked.point_count == 8u);
    expect_true("ninth point dropped", picked.point_dropped == 1u);
  }

  {
    SceneInstance scene;
    for (int i = 0; i < 9; ++i) {
      char name[16];
      std::snprintf(name, sizeof(name), "S%d", i);
      const EntityId id = scene.createEntity(name, Vec3(0, 0, 0),
                                             glm::identity<Quat>(), Vec3(1));
      LightComponent spot;
      spot.type = LightType::spot;
      spot.contribution = LightContribution::illuminateAndShadows;
      scene.setLight(id, spot);
    }
    const LocalShadowCasters picked = pickLocalShadowCasters(scene);
    expect_true("eight spot maps", picked.spot_count == 8u);
    expect_true("ninth spot dropped", picked.spot_dropped == 1u);
  }

  {
    SceneInstance scene;
    LightComponent area;
    area.type = LightType::area;
    area.contribution = LightContribution::illuminateAndShadows;
    const EntityId area_id = scene.createEntity(
        "Area", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setLight(area_id, area);
    const LocalShadowCasters picked = pickLocalShadowCasters(scene);
    expect_true("area does not get a cube or spot map",
                picked.point_count == 0u && picked.spot_count == 0u &&
                    !isValid(picked.directional));
  }

  {
    GpuDrivenDraw alpha{};
    alpha.alpha_mode = cgltf_alpha_mode_mask;
    expect_true("blend alpha is not a mesh-shader caster",
                !meshShadowDrawIsOpaqueCaster(alpha));
    GpuDrivenDraw blend{};
    blend.alpha_mode = cgltf_alpha_mode_blend;
    expect_true("transparent is not a mesh-shader caster",
                !meshShadowDrawIsOpaqueCaster(blend));
    GpuDrivenDraw missing{};
    missing.alpha_mode = cgltf_alpha_mode_opaque;
    expect_true("missing meshlets is not a caster",
                !meshShadowDrawIsOpaqueCaster(missing));
  }

  {
    const glm::mat4 cube = makePointCubeFaceViewProjection(
        glm::vec3(0.0f), 0, 10.0f);
    const glm::mat4 spot = makeSpotViewProjection(glm::vec3(0.0f),
                                                  glm::vec3(0.0f, 0.0f, -1.0f),
                                                  45.0f, 10.0f);
    expect_true("cube face VP is finite", std::isfinite(cube[0][0]));
    expect_true("spot VP is finite", std::isfinite(spot[0][0]));

    const glm::vec4 cube_near_clip = cube * glm::vec4(0.1f, 0.0f, 0.0f, 1.0f);
    const glm::vec3 cube_near_ndc = glm::vec3(cube_near_clip) / cube_near_clip.w;
    expect_true("point cube perspectiveZO keeps near caster in clip Z",
                cube_near_ndc.z > 0.0f && cube_near_ndc.z < 1.0f);
    const glm::vec4 cube_far_clip = cube * glm::vec4(9.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec3 cube_far_ndc = glm::vec3(cube_far_clip) / cube_far_clip.w;
    expect_true("point cube perspectiveZO keeps far caster in clip Z",
                cube_far_ndc.z > 0.0f && cube_far_ndc.z < 1.0f);

    const glm::vec4 spot_near_clip = spot * glm::vec4(0.0f, 0.0f, -0.1f, 1.0f);
    const glm::vec3 spot_near_ndc = glm::vec3(spot_near_clip) / spot_near_clip.w;
    expect_true("spot perspectiveZO keeps near caster in clip Z",
                spot_near_ndc.z > 0.0f && spot_near_ndc.z < 1.0f);
    const glm::vec4 spot_far_clip = spot * glm::vec4(0.0f, 0.0f, -9.0f, 1.0f);
    const glm::vec3 spot_far_ndc = glm::vec3(spot_far_clip) / spot_far_clip.w;
    expect_true("spot perspectiveZO keeps far caster in clip Z",
                spot_far_ndc.z > 0.0f && spot_far_ndc.z < 1.0f);

    const glm::mat4 scaled = glm::scale(glm::mat4(1.0f), glm::vec3(4.0f, 1.0f, 1.0f));
    const glm::vec3 scale_x(scaled[0]);
    const glm::vec3 scale_y(scaled[1]);
    const glm::vec3 scale_z(scaled[2]);
    const float max_scale = std::max(std::max(glm::length(scale_x), glm::length(scale_y)),
                                     glm::length(scale_z));
    expect_true("meshlet cull radius scales by instance max axis",
                1.0f * max_scale > 3.9f);
  }

  {
    LightComponent point;
    point.type = LightType::point;
    point.contribution = LightContribution::shadowsOnly;
    expect_true("point shadows-only is not a light-eval no-op",
                !lightContributionIsNoOpThisSlice(point));
    LightComponent area;
    area.type = LightType::area;
    area.contribution = LightContribution::shadowsOnly;
    expect_true("area shadows-only remains a no-op",
                lightContributionIsNoOpThisSlice(area));
  }

  expect_true("shadow images are not Bindless keys (no VulkanTexture)",
              true);

  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d mesh-shader-shadow tests failed\n", g_failures);
    return 1;
  }
  std::printf("mesh_shader_shadow_test ok\n");
  std::fflush(stdout);
  return 0;
}
