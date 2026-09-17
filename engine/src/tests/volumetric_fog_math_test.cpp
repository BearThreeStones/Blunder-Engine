#include "runtime/core/log/log_system.h"
#include "runtime/core/math/geometry.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/volumetric_fog_math.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/scene/scene_instance.h"

#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
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
    const FroxelGridSize size = froxelGridSize(1920, 1080);
    expect_true("xy tiles 1920", size.x == 120);
    expect_true("xy tiles 1080", size.y == 68);
    expect_true("z slices 64", size.z == 64);
    expect_true("tile 16", k_volumetric_fog_tile_px == 16);
    expect_true("S 32", k_volumetric_fog_distribution_s == 32.0f);
  }

  {
    const float near_z = volumetricFogNear(0.01f);
    expect_true("near offset 9.5cm", float_near(near_z, 0.095f));
    const float far_z = 60.0f;
    const FroxelGridZParams params = makeFroxelGridZParams(near_z, far_z);
    expect_true("params S", float_near(params.z, 32.0f));
    const float z0 = viewZFromFroxelSlice(0.0f, params);
    const float z_far = viewZFromFroxelSlice(63.0f, params);
    expect_true("slice 0 is near", float_near(z0, near_z, 1e-3f));
    expect_true("slice 63 is far", float_near(z_far, far_z, 1e-2f));
    expect_true("round-trip slice 0",
                float_near(froxelSliceFromViewZ(z0, params), 0.0f, 1e-3f));
    expect_true("round-trip slice 63",
                float_near(froxelSliceFromViewZ(z_far, params), 63.0f, 1e-3f));
    const float mid_z = viewZFromFroxelSlice(16.0f, params);
    expect_true("round-trip slice 16",
                float_near(froxelSliceFromViewZ(mid_z, params), 16.0f, 1e-3f));
  }

  {
    const float view_distance = 42.0f;
    const float camera_far = 100000.0f;
    const float metre_far = volumetricFogVolumeFar(view_distance, false);
    const float cm_far = volumetricFogVolumeFar(view_distance, true);
    expect_true("volume far ignores camera far in metres",
                float_near(metre_far, view_distance));
    expect_true("volume far is not Viewport far clip", metre_far < camera_far * 0.01f);
    expect_true("centimetre Sponza volume far is 42m in cm",
                float_near(cm_far, view_distance / 0.008f, 1e-2f));
    const FroxelGridZParams huge = makeFroxelGridZParams(0.095f, camera_far);
    const FroxelGridZParams fog = makeFroxelGridZParams(0.095f, cm_far);
    const float courtyard_z = 3000.0f;
    expect_true("camera-far slices skip the courtyard",
                froxelSliceFromViewZ(courtyard_z, huge) < 8.0f);
    expect_true("Fog Unique slices still cover the courtyard",
                froxelSliceFromViewZ(courtyard_z, fog) > 16.0f);
    expect_true("cm height falloff is per-metre",
                float_near(volumetricFogHeightFalloff(0.32f, true), 0.32f * 0.008f));
    expect_true("metre height falloff stays authored",
                float_near(volumetricFogHeightFalloff(0.32f, false), 0.32f));
  }

  {
    const float rho = 0.02f;
    const float falloff = 0.2f;
    const float fog_h = 1.0f;
    const float at_height = volumetricHeightDensity(rho, falloff, 1.0f, fog_h);
    const float above = volumetricHeightDensity(rho, falloff, 11.0f, fog_h);
    const float same_z_other_y =
        volumetricHeightDensity(rho, falloff, 1.0f, fog_h);
    expect_true("density at fog height is rho", float_near(at_height, rho));
    expect_true("higher Z is thinner", above < at_height);
    expect_true("world Y is not height", float_near(same_z_other_y, at_height));
    std::fprintf(stdout, "height density dump: z=fogHeight %.6g  z=+10m %.6g\n",
                 static_cast<double>(at_height), static_cast<double>(above));
  }

  {
    expect_true("Player + fog on",
                shouldApplyVolumetricFog(EngineHostMode::Player, true));
    expect_true("Editor Viewport + fog on",
                shouldApplyVolumetricFog(EngineHostMode::Editor, true));
    expect_true("Player missing Fog off",
                !shouldApplyVolumetricFog(EngineHostMode::Player, false));
    expect_true("Editor missing Fog off",
                !shouldApplyVolumetricFog(EngineHostMode::Editor, false));
  }

  {
    const glm::vec3 history(1.0f, 1.0f, 1.0f);
    const glm::vec3 current(0.0f, 0.0f, 0.0f);
    const glm::vec3 mixed = temporalScatterMix(history, current, true);
    expect_true("temporal 20/80", float_near(mixed.x, 0.8f) &&
                                      float_near(mixed.y, 0.8f) &&
                                      float_near(mixed.z, 0.8f));
    const glm::vec3 skipped = temporalScatterMix(history, current, false);
    expect_true("skip OOB history", float_near(skipped.x, 0.0f));
  }

  {
    const glm::mat4 dolly_a =
        glm::lookAt(glm::vec3(0.0f, -10.0f, 5.0f), glm::vec3(0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::mat4 dolly_b =
        glm::lookAt(glm::vec3(0.0f, -8.0f, 4.0f), glm::vec3(0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::mat4 orbit =
        glm::lookAt(glm::vec3(10.0f, 0.0f, 5.0f), glm::vec3(0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    expect_true(
        "dolly keeps fog history",
        volumetricFogHistoryUseful(volumetricFogViewForward(dolly_a),
                                   volumetricFogViewForward(dolly_b)));
    expect_true(
        "orbit drops fog history",
        !volumetricFogHistoryUseful(volumetricFogViewForward(dolly_a),
                                    volumetricFogViewForward(orbit)));
  }

  {
    SceneInstance scene;
    const EntityId first =
        scene.createEntity("FogA", Vec3(0, 5, 2), glm::identity<Quat>(), Vec3(1));
    const EntityId second =
        scene.createEntity("FogB", Vec3(0, 9, 8), glm::identity<Quat>(), Vec3(1));
    FogComponent a{};
    a.density = 0.02f;
    scene.setFog(first, a);
    FogComponent b{};
    b.density = 0.9f;
    scene.setFog(second, b);
    scene.tick(0.0f);
    ActiveFog picked = pickActiveFog(scene);
    expect_true("first EntityId wins", picked.entity_id == first);
    expect_true("second Fog ignored", float_near(picked.fog.density, 0.02f));
    expect_true("height is world Z", float_near(picked.world_height_z, 2.0f));

    FogComponent disabled = a;
    disabled.enabled = false;
    scene.setFog(first, disabled);
    picked = pickActiveFog(scene);
    expect_true("disabled first skipped", picked.entity_id == second);

    FogComponent volumetric_off = a;
    volumetric_off.volumetric_enabled = false;
    scene.setFog(first, volumetric_off);
    scene.setFog(second, FogComponent{});
    scene.setObjectActive(second, false);
    picked = pickActiveFog(scene);
    expect_true("inactive / volumetric-off ignored",
                picked.entity_id == k_invalid_entity_id);
  }

  {
    SceneInstance scene;
    const EntityId point =
        scene.createEntity("Point", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId directional =
        scene.createEntity("Sun", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    LightComponent point_light{};
    point_light.type = LightType::point;
    point_light.enabled = true;
    scene.setLight(point, point_light);
    expect_true("points do not inject",
                pickIlluminatingDirectional(scene) == k_invalid_entity_id);

    LightComponent sun{};
    sun.type = LightType::directional;
    sun.enabled = true;
    sun.contribution = LightContribution::illuminateOnly;
    scene.setLight(directional, sun);
    expect_true("illuminating directional picked",
                pickIlluminatingDirectional(scene) == directional);

    LightComponent shadows_only = sun;
    shadows_only.contribution = LightContribution::shadowsOnly;
    scene.setLight(directional, shadows_only);
    expect_true("shadows-only skipped",
                pickIlluminatingDirectional(scene) == k_invalid_entity_id);
  }

  {
    SceneInstance scene;
    const EntityId directional =
        scene.createEntity("Sun", Vec3(0, 0, 8), glm::identity<Quat>(), Vec3(1));
    const EntityId point =
        scene.createEntity("Point", Vec3(-1, 0, 2), glm::identity<Quat>(), Vec3(1));
    const EntityId spot =
        scene.createEntity("Spot", Vec3(0, 0, 6), glm::identity<Quat>(), Vec3(1));
    const EntityId area =
        scene.createEntity("Area", Vec3(2, 0, 2), glm::identity<Quat>(), Vec3(1));
    const EntityId shadows_only =
        scene.createEntity("SpotDark", Vec3(0, 1, 3), glm::identity<Quat>(), Vec3(1));

    LightComponent sun{};
    sun.type = LightType::directional;
    sun.enabled = true;
    scene.setLight(directional, sun);

    LightComponent point_light{};
    point_light.type = LightType::point;
    point_light.enabled = true;
    point_light.contribution = LightContribution::illuminateOnly;
    scene.setLight(point, point_light);

    LightComponent spot_light{};
    spot_light.type = LightType::spot;
    spot_light.enabled = true;
    spot_light.contribution = LightContribution::illuminateOnly;
    scene.setLight(spot, spot_light);

    LightComponent area_light{};
    area_light.type = LightType::area;
    area_light.enabled = true;
    scene.setLight(area, area_light);

    LightComponent hidden{};
    hidden.type = LightType::spot;
    hidden.enabled = true;
    hidden.contribution = LightContribution::shadowsOnly;
    scene.setLight(shadows_only, hidden);

    scene.tick(0.0f);
    EvaluatedLight locals[k_max_volumetric_fog_local_lights];
    const size_t count =
        gatherFogLocalLights(scene, locals, k_max_volumetric_fog_local_lights);
    expect_true("local inject count is point+spot", count == 2);
    expect_true("point packed first by EntityId",
                count >= 1 && locals[0].entity_id == point &&
                    locals[0].type == LightType::point);
    expect_true("spot packed second",
                count >= 2 && locals[1].entity_id == spot &&
                    locals[1].type == LightType::spot);
  }

  {
    // Authored world position is already world units and must survive untouched;
    // only the metre-authored range converts. Parking a courtyard light near the
    // origin used to teleport it 125x away.
    SceneInstance scene;
    const EntityId point = scene.createEntity(
        "West", Vec3(-6.5f, 0.0f, 2.4f), glm::identity<Quat>(), Vec3(1));
    LightComponent point_light{};
    point_light.type = LightType::point;
    point_light.enabled = true;
    point_light.contribution = LightContribution::illuminateOnly;
    point_light.range = 12.0f;
    scene.setLight(point, point_light);
    scene.setWorldBounds(
        AABB{Vec3(-1900.0f, -1150.0f, 0.0f), Vec3(1900.0f, 1150.0f, 1550.0f)});
    scene.tick(0.0f);
    EvaluatedLight locals[k_max_volumetric_fog_local_lights];
    const size_t count =
        gatherFogLocalLights(scene, locals, k_max_volumetric_fog_local_lights);
    expect_true("cm mesh keeps fog light on its authored world position",
                count == 1 && float_near(locals[0].world_position.x, -6.5f) &&
                    float_near(locals[0].world_position.z, 2.4f));
    expect_true("cm mesh scales fog range to centimetres",
                count == 1 && float_near(locals[0].range, 12.0f / 0.008f, 1e-2f));
    expect_true("fog inject keeps authored intensity",
                count == 1 &&
                    float_near(locals[0].color_times_intensity.x,
                               point_light.color.x * point_light.intensity));
  }

  {
    // Fog density is per metre but integrated over world-unit slice depths.
    // Sponza (1 unit = 8 mm) reached optical depth ~486 over the 42 m volume,
    // so transmittance collapsed in the first froxel and the composite dropped
    // the scene: a flat wash instead of corridor shafts.
    const float authored_density = 0.09f;
    const float cm_density = volumetricFogDensity(authored_density, true);
    expect_true("metre world keeps authored density",
                float_near(volumetricFogDensity(authored_density, false),
                           authored_density));
    expect_true("cm world density is per world unit",
                float_near(cm_density, authored_density * 0.008f, 1e-6f));

    const float far_z = volumetricFogVolumeFar(42.0f, true);
    const FroxelGridZParams zp = makeFroxelGridZParams(volumetricFogNear(0.1f), far_z);
    const auto volume_optical_depth = [&](float density) {
      float tau = 0.0f;
      for (uint32_t s = 0; s < k_volumetric_fog_slice_count; ++s) {
        const float z0 = viewZFromFroxelSlice(static_cast<float>(s), zp);
        const float z1 = viewZFromFroxelSlice(static_cast<float>(s) + 1.0f, zp);
        tau += density * std::max(z1 - z0, 1e-4f);
      }
      return tau;
    };
    expect_true("unconverted density saturates the whole volume",
                volume_optical_depth(authored_density) > 100.0f);
    const float tau = volume_optical_depth(cm_density);
    expect_true("converted density matches density x viewDistance",
                tau > 3.0f && tau < 5.0f);
    expect_true("converted density leaves transmittance to grade",
                std::exp(-tau) > 1e-3f);
  }

  {
    const float at_origin = volumetricFogLocalAtten(0.01f, 10.0f);
    const float at_radius =
        volumetricFogLocalAtten(k_volumetric_fog_local_source_radius_m, 10.0f);
    const float far = volumetricFogLocalAtten(11.0f, 10.0f);
    expect_true("fog local near field is bounded", at_origin < 1.01f);
    expect_true("fog local clamps to source radius",
                float_near(at_origin, at_radius));
    expect_true("fog local beyond range is zero", far == 0.0f);
    expect_true("fog local is not inverse-square",
                at_origin < punctualRangeAttenuation(0.01f, 10.0f) * 0.01f);
  }

  {
    SlangCompiler compiler;
    compiler.initialize();
    const auto density = compiler.compileComputeProgram(
        "engine/shaders/volumetric_fog_density.slang");
    const auto scatter = compiler.compileComputeProgram(
        "engine/shaders/volumetric_fog_scatter.slang");
    const auto integrate = compiler.compileComputeProgram(
        "engine/shaders/volumetric_fog_integrate.slang");
    const auto composite = compiler.compileGraphicsProgram(
        "engine/shaders/volumetric_fog_composite.slang");
    expect_true("density spirv", !density.compute.spirv_code.empty());
    expect_true("scatter spirv", !scatter.compute.spirv_code.empty());
    expect_true("integrate spirv", !integrate.compute.spirv_code.empty());
    expect_true("composite spirv",
                !composite.vertex.spirv_code.empty() &&
                    !composite.fragment.spirv_code.empty());
    compiler.shutdown();
  }

  g_runtime_global_context.m_logger_system.reset();
  if (g_failures != 0) {
    std::fprintf(stderr, "volumetric_fog_math_test: %d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "volumetric_fog_math_test: all passed\n");
  return 0;
}
