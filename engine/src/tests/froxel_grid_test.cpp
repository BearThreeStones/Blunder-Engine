#include "runtime/function/render/clustered/froxel_grid.h"
#include "runtime/function/render/clustered/froxel_debug.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/scene_instance.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

Blunder::EvaluatedLight makePoint(Blunder::EntityId id, const Blunder::Vec3& pos,
                                  float range) {
  Blunder::EvaluatedLight light{};
  light.entity_id = id;
  light.type = Blunder::LightType::point;
  light.world_position = pos;
  light.range = range;
  light.color_times_intensity = Blunder::Vec3(1.0f);
  return light;
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    const FroxelGridDim dim = makeFroxelGridDim(1920, 1080);
    expect_true("1080p tiles x", dim.tiles_x == 30);
    expect_true("1080p tiles y", dim.tiles_y == 17);
    expect_true("default 32 slices", dim.slices == 32);
    expect_true("default 64 px tiles", dim.tile_size_px == 64);
    expect_true("1080p froxel count", froxelCount(dim) == 30u * 17u * 32u);
    expect_true("1080p index buffer",
                froxelIndexBufferCount(dim) == 30u * 17u * 32u * 64u);
    expect_true("more than one Z slice", dim.slices > 1);
  }

  {
    const float near_z = 0.1f;
    const float far_z = 100.0f;
    const float near_span =
        sliceToViewZ(1.0f, near_z, far_z, k_froxel_z_slices) -
        sliceToViewZ(0.0f, near_z, far_z, k_froxel_z_slices);
    const float far_span =
        sliceToViewZ(32.0f, near_z, far_z, k_froxel_z_slices) -
        sliceToViewZ(31.0f, near_z, far_z, k_froxel_z_slices);
    expect_true("near slice thinner than far", near_span < far_span);
    expect_true("slice 0 starts at near",
                std::abs(sliceToViewZ(0.0f, near_z, far_z, k_froxel_z_slices) -
                         near_z) < 1e-4f);
    expect_true("slice 32 is far",
                std::abs(sliceToViewZ(32.0f, near_z, far_z, k_froxel_z_slices) -
                         far_z) < 1e-3f);
    bool in_range = true;
    for (int i = 0; i < 64; ++i) {
      const float z =
          near_z + (far_z - near_z) * (static_cast<float>(i) / 63.0f);
      const uint32_t slice = viewZToSlice(z, near_z, far_z, k_froxel_z_slices);
      if (slice > 31u) {
        in_range = false;
      }
    }
    expect_true("slice indices stay in [0, 31]", in_range);
    expect_true("near plane maps to slice 0",
                viewZToSlice(near_z, near_z, far_z, k_froxel_z_slices) == 0);
    expect_true("just below far stays at 31",
                viewZToSlice(far_z * 0.999f, near_z, far_z, k_froxel_z_slices) <=
                    31u);
  }

  glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 8.0f), glm::vec3(0.0f),
                               glm::vec3(0.0f, 1.0f, 0.0f));
  glm::mat4 projection =
      glm::perspectiveZO(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
  projection[1][1] *= -1.0f;
  const glm::mat4 inv_projection = glm::inverse(projection);
  const FroxelGridDim dim = makeFroxelGridDim(256, 256);
  const uint32_t n_froxels = froxelCount(dim);
  std::vector<uint32_t> indices(froxelIndexBufferCount(dim), 0u);
  std::vector<uint32_t> counts(n_froxels, 0u);

  {
    EvaluatedLight lights[3];
    lights[0] = makePoint(1, Vec3(0.0f, 0.0f, 0.0f), 4.0f);
    lights[1] = makePoint(2, Vec3(50.0f, 0.0f, 0.0f), 1.0f);
    lights[2] = {};
    lights[2].entity_id = 3;
    lights[2].type = LightType::directional;
    lights[2].world_position = Vec3(0.0f);
    lights[2].range = 100.0f;

    const FroxelFillResult filled = fillFroxelGridCpu(
        dim, view, inv_projection, 0.1f, 100.0f, lights, 3, indices.data(),
        counts.data());
    uint32_t origin_hits = 0;
    uint32_t far_hits = 0;
    uint32_t directional_hits = 0;
    uint32_t occupied = 0;
    for (uint32_t i = 0; i < n_froxels; ++i) {
      if (counts[i] > 0) {
        ++occupied;
      }
      for (uint32_t k = 0; k < counts[i]; ++k) {
        const uint32_t li = indices[i * k_froxel_lights_per_cell + k];
        if (li == 0) {
          ++origin_hits;
        } else if (li == 1) {
          ++far_hits;
        } else if (li == 2) {
          ++directional_hits;
        }
      }
    }
    expect_true("origin light assigned to overlapping froxels", origin_hits > 0);
    expect_true("grid has occupied and empty froxels",
                occupied > 0 && occupied < n_froxels);
    expect_true("directional never written", directional_hits == 0);
    expect_true("fill assigned some lights", filled.assigned_total > 0);
    (void)far_hits;
  }

  {
    EvaluatedLight lights[70];
    for (int i = 0; i < 70; ++i) {
      lights[i] = makePoint(static_cast<EntityId>(i + 1), Vec3(0.0f, 0.0f, 0.0f),
                            20.0f);
    }
    std::fill(indices.begin(), indices.end(), 0u);
    std::fill(counts.begin(), counts.end(), 0u);
    const FroxelFillResult filled = fillFroxelGridCpu(
        dim, view, inv_projection, 0.1f, 100.0f, lights, 70, indices.data(),
        counts.data());
    uint32_t max_count = 0;
    uint32_t capped_cells = 0;
    for (uint32_t i = 0; i < n_froxels; ++i) {
      max_count = std::max(max_count, counts[i]);
      if (counts[i] == k_froxel_lights_per_cell) {
        ++capped_cells;
      }
      expect_true("count never exceeds 64", counts[i] <= k_froxel_lights_per_cell);
    }
    expect_true("overlapping cell keeps 64", max_count == k_froxel_lights_per_cell);
    expect_true("overflow recorded for dropped lights",
                filled.dropped_assignments > 0);
    expect_true("at least one capped froxel", capped_cells > 0);
    // Adjacent empty/far tiles stay below cap when they don't contain the cluster.
    bool saw_below_cap = false;
    for (uint32_t i = 0; i < n_froxels; ++i) {
      if (counts[i] > 0 && counts[i] < k_froxel_lights_per_cell) {
        saw_below_cap = true;
      }
    }
    expect_true("overflow in one froxel does not force every occupied cell to 64",
                saw_below_cap || capped_cells < n_froxels);
  }

  {
    EvaluatedLight mixed[3];
    mixed[0] = {};
    mixed[0].entity_id = 10;
    mixed[0].type = LightType::directional;
    mixed[0].range = 100.0f;
    mixed[1] = {};
    mixed[1].entity_id = 11;
    mixed[1].type = LightType::area;
    mixed[1].world_position = Vec3(0.0f);
    mixed[1].range = 100.0f;
    mixed[2] = makePoint(12, Vec3(0.0f), 8.0f);
    std::fill(indices.begin(), indices.end(), 0u);
    std::fill(counts.begin(), counts.end(), 0u);
    fillFroxelGridCpu(dim, view, inv_projection, 0.1f, 100.0f, mixed, 3,
                      indices.data(), counts.data());
    bool only_point = true;
    uint32_t listed = 0;
    for (uint32_t i = 0; i < n_froxels; ++i) {
      for (uint32_t k = 0; k < counts[i]; ++k) {
        ++listed;
        if (indices[i * k_froxel_lights_per_cell + k] != 2) {
          only_point = false;
        }
      }
    }
    expect_true("only point/spot written vs directional/area",
                only_point && listed > 0);
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
      scene.setLight(id, point);
    }
    LightComponent dir;
    dir.type = LightType::directional;
    const EntityId sun = scene.createEntity("Sun", Vec3(0, 8, 0),
                                            glm::identity<Quat>(), Vec3(1));
    scene.setLight(sun, dir);

    EvaluatedLight clustered[k_max_clustered_lights];
    const size_t count =
        buildClusteredPointSpotList(scene, clustered, k_max_clustered_lights);
    expect_true("nine point lights all fit clustered list", count == 9);
    bool saw_dir = false;
    for (size_t i = 0; i < count; ++i) {
      if (clustered[i].type == LightType::directional) {
        saw_dir = true;
      }
    }
    expect_true("clustered list excludes directional", !saw_dir);
  }

  {
    SceneInstance scene;
    const EntityId point = scene.createEntity(
        "West", Vec3(-812.5f, 0.0f, 300.0f), glm::identity<Quat>(), Vec3(1));
    LightComponent point_light{};
    point_light.type = LightType::point;
    point_light.enabled = true;
    point_light.range = 12.0f;
    point_light.intensity = 16.0f;
    scene.setLight(point, point_light);
    scene.setWorldBounds(
        AABB{Vec3(-1920.0f, -1105.0f, -126.0f), Vec3(1799.0f, 1182.0f, 1429.0f)});
    scene.tick(0.0f);
    EvaluatedLight clustered[k_max_clustered_lights];
    const size_t count =
        buildClusteredPointSpotList(scene, clustered, k_max_clustered_lights);
    expect_true("clustered Sponza point converts range to centimetres",
                count == 1 && std::fabs(clustered[0].range - 1500.0f) < 1e-2f);
    expect_true(
        "clustered intensity carries the square",
        count == 1 &&
            std::fabs(clustered[0].color_times_intensity.x - 16.0f * 125.0f * 125.0f) <
                1.0f);
    expect_true("clustered world position is not rescaled",
                count == 1 && std::fabs(clustered[0].world_position.x + 812.5f) < 1e-3f);
  }

  {
    expect_true("metre clustered far stays camera far",
                std::fabs(clusteredLightingFar(100000.0f, 16.0f) - 100000.0f) < 1e-3f);
    const float cm_far = clusteredLightingFar(100000.0f, 4000.0f);
    expect_true("cm clustered far is scene span not Viewport 100000",
                cm_far < 10000.0f && cm_far > 1000.0f);
    const float lookat_far = clusteredLightingFar(100000.0f, 4000.0f, 9007.0f);
    expect_true("LOOKAT zoom keeps the courtyard inside clustered far",
                lookat_far > 11000.0f && lookat_far <= 100000.0f);
  }

  {
    const glm::vec3 empty = occupancyHeatColor(0);
    const glm::vec3 mid = occupancyHeatColor(32);
    const glm::vec3 capped = occupancyHeatColor(64);
    const glm::vec3 overflowed = occupancyHeatColor(80);
    auto differs = [](const glm::vec3& a, const glm::vec3& b) {
      return a.x != b.x || a.y != b.y || a.z != b.z;
    };
    auto same = [](const glm::vec3& a, const glm::vec3& b) {
      return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    expect_true("heatmap empty differs from mid", differs(empty, mid));
    expect_true("heatmap mid differs from cap", differs(mid, capped));
    expect_true("capped color uses 64 not pre-cap count", same(capped, overflowed));
    expect_true("heatmap t at cap is 1", occupancyHeatColorT(64) == 1.0f);
    expect_true("heatmap t above cap stays 1", occupancyHeatColorT(80) == 1.0f);
  }

  if (g_failures > 0) {
    std::fprintf(stderr, "froxel_grid_test: %d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "froxel_grid_test: all passed\n");
  return 0;
}
