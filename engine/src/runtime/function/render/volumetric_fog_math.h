#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/fog_component.h"
#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/light_component.h"

namespace Blunder {

class SceneInstance;

constexpr uint32_t k_volumetric_fog_tile_px = 16;
constexpr uint32_t k_volumetric_fog_slice_count = 64;
constexpr float k_volumetric_fog_distribution_s = 32.0f;
constexpr float k_volumetric_fog_near_offset_m = 0.095f;
constexpr float k_volumetric_fog_temporal_current = 0.2f;
/// Reproject scatter history only when view forward is stable. Orbit yaws
/// throw most froxels OOB; dolly keeps the same rays so 20/80 reuse pays.
constexpr float k_volumetric_fog_history_min_forward_dot = 0.999f;
/// Unshadowed point/spot inject cap. Directional stays on the dedicated UBO fields.
constexpr uint32_t k_max_volumetric_fog_local_lights = 8;
/// Fog inject skips surface 1/r^2 so locals read as shafts/washes, not UNORM orbs.
constexpr float k_volumetric_fog_local_source_radius_m = 1.25f;
constexpr float k_volumetric_fog_local_scatter_scale = 0.18f;

struct FroxelGridSize {
  uint32_t x{1};
  uint32_t y{1};
  uint32_t z{k_volumetric_fog_slice_count};
};

/// Unreal CalculateGridZParams: slice = log2(z * X + Y) * Z, Z = S.
struct FroxelGridZParams {
  float x{1.0f};
  float y{0.0f};
  float z{k_volumetric_fog_distribution_s};
};

struct ActiveFog {
  EntityId entity_id{k_invalid_entity_id};
  FogComponent fog{};
  float world_height_z{0.0f};
};

inline uint32_t froxelAxisCount(uint32_t view_extent_px) {
  const uint32_t extent = std::max(1u, view_extent_px);
  return (extent + k_volumetric_fog_tile_px - 1u) / k_volumetric_fog_tile_px;
}

inline FroxelGridSize froxelGridSize(uint32_t view_width, uint32_t view_height) {
  FroxelGridSize size;
  size.x = froxelAxisCount(view_width);
  size.y = froxelAxisCount(view_height);
  size.z = k_volumetric_fog_slice_count;
  return size;
}

inline float volumetricFogNear(float play_near) {
  return std::max(play_near, k_volumetric_fog_near_offset_m);
}

/// 64 exponential slices stay on Fog Unique `viewDistance` (metre-authored).
/// Never use camera far (Viewport 100000 after the zoom raise) — that parks
/// near-camera density in empty air. Centimetre mesh worlds convert metres→cm.
inline float volumetricFogVolumeFar(float view_distance, bool centimetre_world) {
  return gltfMetreQuantityInWorld(std::max(view_distance, 1e-3f), centimetre_world);
}

inline float volumetricFogHeightFalloff(float falloff, bool centimetre_world) {
  return centimetre_world ? falloff * kGltfCentimeterToMeterScale : falloff;
}

/// Fog Unique `density` is an extinction coefficient per **metre**, but
/// `volumetric_fog_integrate.slang` multiplies it by froxel slice depths in
/// world units. Centimetre Sponza needs per-world-unit density or the volume
/// reaches optical depth ~486 instead of ~3.8: transmittance hits 0 in the first
/// froxel, the composite drops the scene entirely and every camera inside the
/// fog layer sees a flat wash instead of corridor shafts.
inline float volumetricFogDensity(float density, bool centimetre_world) {
  return centimetre_world ? density * kGltfCentimeterToMeterScale : density;
}

inline float volumetricFogLocalSourceRadius(bool centimetre_world) {
  return gltfMetreQuantityInWorld(k_volumetric_fog_local_source_radius_m,
                                  centimetre_world);
}

inline FroxelGridZParams makeFroxelGridZParams(float near_z, float far_z,
                                               uint32_t slice_count = k_volumetric_fog_slice_count,
                                               float s = k_volumetric_fog_distribution_s) {
  const float n = std::max(near_z, 1e-4f);
  const float f = std::max(far_z, n + 1e-3f);
  const float slices = std::max(2u, slice_count);
  const float e = std::exp2((static_cast<float>(slices) - 1.0f) / s);
  FroxelGridZParams params;
  params.x = (e - 1.0f) / (f - n);
  params.y = 1.0f - n * params.x;
  params.z = s;
  return params;
}

inline float viewZFromFroxelSlice(float slice, const FroxelGridZParams& params) {
  return (std::exp2(slice / params.z) - params.y) / params.x;
}

inline float froxelSliceFromViewZ(float view_z, const FroxelGridZParams& params) {
  return std::log2(std::max(view_z * params.x + params.y, 1e-6f)) * params.z;
}

/// ρ · exp2(−falloff · (world.z − fogHeight)). World Y is not the height axis.
inline float volumetricHeightDensity(float density, float height_falloff,
                                     float world_z, float fog_height_z) {
  return density * std::exp2(-height_falloff * (world_z - fog_height_z));
}

/// Editor Viewport and Player present paths share `recordViewportGraph`.
/// Camera Preview / Placement Preview / Mesh Preview / Scene Thumbnail do not.
inline bool shouldApplyVolumetricFog(EngineHostMode host_mode, bool has_active_fog) {
  return has_active_fog && (host_mode == EngineHostMode::Editor ||
                            host_mode == EngineHostMode::Player);
}

inline float henyeyGreensteinPhase(float cos_theta, float g) {
  const float g2 = g * g;
  const float denom = 1.0f + g2 - 2.0f * g * cos_theta;
  return (1.0f - g2) / (4.0f * 3.14159265358979323846f * denom * std::sqrt(std::max(denom, 1e-6f)));
}

inline glm::vec3 temporalScatterMix(const glm::vec3& history, const glm::vec3& current,
                                    bool history_in_frustum) {
  if (!history_in_frustum) {
    return current;
  }
  return glm::mix(history, current, k_volumetric_fog_temporal_current);
}

/// Camera -Z in world from a view matrix. Used to tell orbit (forward moves)
/// from dolly (forward stays).
inline glm::vec3 volumetricFogViewForward(const glm::mat4& view) {
  return glm::vec3(-view[0][2], -view[1][2], -view[2][2]);
}

inline bool volumetricFogHistoryUseful(const glm::vec3& prev_forward,
                                       const glm::vec3& forward) {
  const float prev_len2 = glm::dot(prev_forward, prev_forward);
  const float len2 = glm::dot(forward, forward);
  if (prev_len2 < 1e-12f || len2 < 1e-12f) {
    return false;
  }
  const float den = std::sqrt(prev_len2 * len2);
  return glm::dot(prev_forward, forward) >=
         k_volumetric_fog_history_min_forward_dot * den;
}

/// Windowed range falloff with a source radius. No inverse-square (that saturates
/// UNORM composite into white/colored orbs at each point/spot origin).
inline float volumetricFogLocalAtten(float distance, float range) {
  if (range <= 1e-6f) {
    return 0.0f;
  }
  const float d = std::max(distance, k_volumetric_fog_local_source_radius_m);
  if (d >= range) {
    return 0.0f;
  }
  const float t = 1.0f - d / range;
  return t * t;
}

ActiveFog pickActiveFog(const SceneInstance& scene);
EntityId pickIlluminatingDirectional(const SceneInstance& scene);

struct EvaluatedLight;

/// Illuminating point/spot lights in EntityId order. Skips directional/area and
/// shadows-only. Cap is `out_capacity` (scatter uses `k_max_volumetric_fog_local_lights`).
size_t gatherFogLocalLights(const SceneInstance& scene, EvaluatedLight* out_lights,
                            size_t out_capacity);

}  // namespace Blunder
