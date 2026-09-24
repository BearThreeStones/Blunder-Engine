#include "runtime/resource/asset/mesh_material_override.h"

#include "runtime/function/editor/document_history.h"
#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"

#include "EASTL/unique_ptr.h"

#include <cmath>
#include <cstdio>

#include <glm/ext/vector_uint4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void overlayScalarsAndClearSlot() {
  using namespace Blunder;
  Asset::Meta meta;
  meta.virtual_path = "assets/Meshes/hero.mesh.yaml#mat";
  auto material = eastl::make_shared<MaterialAsset>(
      eastl::move(meta), glm::vec4(0.2f, 0.3f, 0.4f, 1.0f), AssetHandle{},
      nullptr, nullptr, nullptr, nullptr, glm::vec3(0.1f), glm::vec3(0.8f),
      glm::vec3(0.2f), 16.0f, 0.1f, 0.9f, cgltf_alpha_mode_opaque, 0.5f, false,
      false);

  MeshMaterialOverride overlay{};
  overlay.unlit.present = true;
  overlay.unlit.value = true;
  overlay.diffuse.present = true;
  overlay.diffuse.value = glm::vec3(1.0f, 0.0f, 0.0f);
  overlay.shininess.present = true;
  overlay.shininess.value = 48.0f;
  overlay.base_color_texture.present = true;
  overlay.base_color_texture.guid.clear();

  applyMeshMaterialOverride(*material, overlay, {});
  expect_true("overlay unlit", material->isUnlit());
  expect_true("overlay diffuse r", material->getDiffuseColor().x == 1.0f);
  expect_true("overlay shininess", material->getShininess() == 48.0f);
  expect_true("empty slot clears base color texture",
              !material->hasBaseColorTexture());
  expect_true("untouched metallic stays import",
              material->getMetallicFactor() == 0.1f);
}

void applyBlinnPhongIgnoresEditorBag() {
  using namespace Blunder;
  Asset::Meta meta;
  meta.virtual_path = "assets/Meshes/hero.mesh.yaml#mat";
  MaterialAsset material(eastl::move(meta), glm::vec4(1.0f), AssetHandle{},
                         nullptr, nullptr, nullptr, nullptr, glm::vec3(0.05f),
                         glm::vec3(0.1f, 0.2f, 0.3f), glm::vec3(0.7f), 64.0f,
                         1.0f, 1.0f, cgltf_alpha_mode_opaque, 0.5f, false,
                         false);

  BlinnPhongEditorSettings editor{};
  editor.diffuse_color = glm::vec3(9.0f);
  editor.specular_color = glm::vec3(8.0f);
  editor.shininess = 4.0f;
  editor.unlit = true;
  editor.ambient_color = glm::vec3(0.9f);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = false;
  applyBlinnPhongToMeshUniforms(ubo, &material, editor, frame);

  expect_true("kd from material not editor",
              ubo.diffuse_color.x == 0.1f && ubo.diffuse_color.y == 0.2f);
  expect_true("shininess from material",
              ubo.specular_color_and_shininess.w == 64.0f);
  expect_true("unlit from material", ubo.material_flags.x == 0.0f);
  expect_true("studio ka from material", ubo.ambient_color.x == 0.05f);

  ForwardMeshUniformData defaults{};
  applyBlinnPhongToMeshUniforms(defaults, nullptr, editor, frame);
  expect_true("null material white kd", defaults.diffuse_color.x == 1.0f);
  expect_true("null material spec 0.4",
              defaults.specular_color_and_shininess.x == 0.4f);
  expect_true("null material shininess 32",
              defaults.specular_color_and_shininess.w == 32.0f);
  expect_true("null material ka 0", defaults.ambient_color.x == 0.0f);
  expect_true("null material not unlit", defaults.material_flags.x == 0.0f);
}

void gltfSpecDefaultMetalWithoutMrMapShadesAsDielectric() {
  using namespace Blunder;
  Asset::Meta meta;
  meta.virtual_path = "assets/Meshes/chocomel.mesh.yaml#mat";
  MaterialAsset omitted_factors(
      eastl::move(meta), glm::vec4(1.0f), AssetHandle{}, nullptr, nullptr, nullptr,
      nullptr, glm::vec3(0.15f), glm::vec3(1.0f), glm::vec3(0.4f), 32.0f, 1.0f,
      1.0f, cgltf_alpha_mode_opaque, 0.5f, false, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &omitted_factors, {}, frame, cgltf_alpha_mode_opaque,
                         0.5f, false);
  expect_true("omitted metal/rough without MR map is dielectric",
              ubo.metallic_roughness_factors.x < 0.01f);
  expect_true("omitted roughness stays 1",
              ubo.metallic_roughness_factors.y > 0.99f);

  Asset::Meta chrome_meta;
  chrome_meta.virtual_path = "assets/Meshes/chrome.mesh.yaml#mat";
  MaterialAsset chrome(eastl::move(chrome_meta), glm::vec4(1.0f), AssetHandle{},
                       nullptr, nullptr, nullptr, nullptr, glm::vec3(0.0f),
                       glm::vec3(1.0f), glm::vec3(1.0f), 256.0f, 1.0f, 0.2f,
                       cgltf_alpha_mode_opaque, 0.5f, false, false);
  ForwardMeshUniformData chrome_ubo{};
  applyPbrToMeshUniforms(chrome_ubo, &chrome, {}, frame, cgltf_alpha_mode_opaque,
                         0.5f, false);
  expect_true("authored metal roughness is kept",
              chrome_ubo.metallic_roughness_factors.x > 0.99f &&
                  std::fabs(chrome_ubo.metallic_roughness_factors.y - 0.2f) <
                      1e-4f);
  expect_true("factor-only material skips albedo bindless",
              ubo.material_flags.y == 0.0f);
}

void untexturedMaterialDoesNotSampleAlbedoBindless() {
  using namespace Blunder;
  Asset::Meta factor_meta;
  factor_meta.virtual_path = "assets/Meshes/snow_patch.mesh.yaml#mat";
  MaterialAsset factor_only(
      eastl::move(factor_meta), glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), AssetHandle{},
      nullptr, nullptr, nullptr, nullptr, glm::vec3(0.15f), glm::vec3(1.0f),
      glm::vec3(0.04f), 156.8f, 0.0f, 0.4f, cgltf_alpha_mode_opaque, 0.5f, true,
      false);

  ForwardMeshUniformData factor_ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(factor_ubo, &factor_only, {}, frame,
                         cgltf_alpha_mode_opaque, 0.5f, true);
  expect_true("snow patch factor-only albedo flag off",
              factor_ubo.material_flags.y == 0.0f);

  Asset::Meta tex_meta;
  tex_meta.virtual_path = "resources/se-world/snow.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(tex_meta), 1u, 1u, 4u, eastl::vector<uint8_t>{255, 255, 255, 255});
  Asset::Meta mat_meta;
  mat_meta.virtual_path = "assets/Meshes/pine.mesh.yaml#mat";
  MaterialAsset textured(eastl::move(mat_meta), glm::vec4(1.0f), AssetHandle{},
                         albedo, nullptr, nullptr, nullptr, glm::vec3(0.15f),
                         glm::vec3(1.0f), glm::vec3(0.4f), 32.0f, 0.0f, 1.0f,
                         cgltf_alpha_mode_opaque, 0.5f, false, false);
  ForwardMeshUniformData textured_ubo{};
  applyPbrToMeshUniforms(textured_ubo, &textured, {}, frame,
                         cgltf_alpha_mode_opaque, 0.5f, false);
  expect_true("textured material albedo flag on",
              textured_ubo.material_flags.y == 1.0f);
}

void foliageMrMapWithZeroMetallicStaysDielectric() {
  using namespace Blunder;
  Asset::Meta mr_meta;
  mr_meta.virtual_path =
      "resources/se-world/assets/lib/textures/pine_leaves_roughness_01.png";
  auto mr = eastl::make_shared<Texture2DAsset>(
      eastl::move(mr_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{255, 205, 255, 255});
  Asset::Meta mat_meta;
  mat_meta.virtual_path = "assets/Meshes/pine.mesh.yaml#mat";
  MaterialAsset foliage(eastl::move(mat_meta), glm::vec4(1.0f), AssetHandle{},
                        nullptr, mr, nullptr, nullptr, glm::vec3(0.15f),
                        glm::vec3(1.0f), glm::vec3(0.04f), 32.0f, 0.0f, 1.0f,
                        cgltf_alpha_mode_mask, 0.5f, true, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &foliage, {}, frame, cgltf_alpha_mode_mask, 0.5f,
                         true);
  expect_true("foliage MR map still sampled", ubo.pbr_texture_flags.x == 1.0f);
  expect_true("foliage extras metallic 0 stays dielectric",
              ubo.metallic_roughness_factors.x < 0.01f);
  expect_true("foliage MASK alpha mode packed",
              std::fabs(ubo.metallic_roughness_factors.w - 1.0f) < 1e-4f);
  expect_true("foliage roughness atlas skips ORM metal",
              ubo.material_flags.z > 0.5f);
  expect_true("foliage cards are two-sided", ubo.pbr_texture_flags.w > 0.5f);
  expect_true("foliage cards pack unlit paper", ubo.material_flags.x > 0.5f);
}

void foliageSpecDefaultMetalWithRoughnessAtlasBecomesDielectricAtPack() {
  using namespace Blunder;
  Asset::Meta mr_meta;
  mr_meta.virtual_path =
      "resources/se-world/assets/lib/textures/pine_leaves_roughness_01.png";
  auto mr = eastl::make_shared<Texture2DAsset>(
      eastl::move(mr_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{255, 205, 255, 255});
  Asset::Meta albedo_meta;
  albedo_meta.virtual_path =
      "resources/se-world/assets/lib/textures/pine_leaves_albedo_01.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(albedo_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{160, 176, 140, 255});
  AssetHandle albedo_handle;
  albedo_handle.type = Asset::Type::Texture2D;
  albedo_handle.key = albedo->getVirtualPath();
  Asset::Meta mat_meta;
  mat_meta.virtual_path = "assets/Meshes/pine.mesh.yaml#mat";
  MaterialAsset foliage(eastl::move(mat_meta), glm::vec4(1.0f), albedo_handle,
                        albedo, mr, nullptr, nullptr, glm::vec3(0.15f),
                        glm::vec3(1.0f), glm::vec3(0.04f), 32.0f, 1.0f, 1.0f,
                        cgltf_alpha_mode_opaque, 0.5f, false, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &foliage, {}, frame, cgltf_alpha_mode_opaque, 0.5f,
                         false);
  expect_true("GPU pack zeros spec-default metal on roughness atlas",
              ubo.metallic_roughness_factors.x < 0.01f);
  expect_true("GPU pack skips ORM B", ubo.material_flags.z > 0.5f);
  expect_true("GPU pack MASK cutout even if draw alpha was opaque",
              std::fabs(ubo.metallic_roughness_factors.w - 1.0f) < 1e-4f);
  expect_true("GPU pack two-sided foliage cards",
              ubo.pbr_texture_flags.w > 0.5f);
  expect_true("GPU pack unlit paper foliage", ubo.material_flags.x > 0.5f);

  // Bindless overwrite used to force ORM sampling; z must survive.
  ubo.pbr_texture_flags.x = 1.0f;
  expect_true("bindless MR flag does not clear roughness-only",
              ubo.material_flags.z > 0.5f);
  expect_true("metallic stays 0 after bindless MR flag",
              ubo.metallic_roughness_factors.x < 0.01f);
}

void paperGrainNormalIsNotSampledAndPaperColorTints() {
  using namespace Blunder;
  Asset::Meta nrm_meta;
  nrm_meta.virtual_path = "resources/se-world/assets/textures/paper_rough_256.png";
  auto paper_rough = eastl::make_shared<Texture2DAsset>(
      eastl::move(nrm_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{128, 128, 128, 255});
  Asset::Meta mr_meta;
  mr_meta.virtual_path =
      "resources/se-world/assets/lib/textures/pine_leaves_roughness_01.png";
  auto mr = eastl::make_shared<Texture2DAsset>(
      eastl::move(mr_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{255, 205, 255, 255});
  Asset::Meta albedo_meta;
  albedo_meta.virtual_path =
      "resources/se-world/assets/lib/textures/pine_leaves_albedo_01.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(albedo_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{160, 176, 140, 255});
  AssetHandle albedo_handle;
  albedo_handle.type = Asset::Type::Texture2D;
  albedo_handle.key = albedo->getVirtualPath();
  Asset::Meta mat_meta;
  mat_meta.virtual_path = "assets/Meshes/pine.mesh.yaml#mat";
  MaterialAsset foliage(eastl::move(mat_meta), glm::vec4(1.0f), albedo_handle,
                        albedo, mr, paper_rough, nullptr, glm::vec3(0.15f),
                        glm::vec3(1.0f), glm::vec3(0.04f), 32.0f, 0.0f, 1.0f,
                        cgltf_alpha_mode_mask, 0.5f, true, false);
  foliage.setPaperColor(glm::vec3(0.65f, 0.69f, 0.57f));

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &foliage, {}, frame, cgltf_alpha_mode_mask, 0.5f,
                         true);
  expect_true("paper grain not sampled as tangent normal",
              ubo.pbr_texture_flags.y < 0.5f);
  expect_true("paper card unlit", ubo.material_flags.x > 0.5f);
  expect_true("paper_color tints albedo r",
              std::fabs(ubo.base_color_factor.x - 0.65f) < 1e-4f);
  expect_true("paper_color tints albedo g",
              std::fabs(ubo.base_color_factor.y - 0.69f) < 1e-4f);

  glm::uvec4 bindless{3u, 4u, 5u, 0u};
  applyBindlessPbrMapFlags(ubo.pbr_texture_flags, bindless, ubo.material_flags);
  expect_true("bindless paper_rough cannot re-enable normals",
              ubo.pbr_texture_flags.y < 0.5f);
  expect_true("bindless still samples roughness atlas",
              ubo.pbr_texture_flags.x > 0.5f);
}

void dummyPathAlbedoUriIsUnlitPaperMask() {
  using namespace Blunder;
  Asset::Meta albedo_meta;
  albedo_meta.virtual_path =
      "resources/se-world/assets/lib/textures/path_7_albedo.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(albedo_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{140, 95, 70, 255});
  AssetHandle albedo_handle;
  albedo_handle.type = Asset::Type::Texture2D;
  albedo_handle.key = albedo->getVirtualPath();
  Asset::Meta mat_meta;
  mat_meta.virtual_path = "assets/Meshes/se-world/SL-fence-paths-m1p0.mesh.yaml#mat";
  MaterialAsset path(eastl::move(mat_meta), glm::vec4(1.0f), albedo_handle,
                     albedo, nullptr, nullptr, nullptr, glm::vec3(0.15f),
                     glm::vec3(1.0f), glm::vec3(0.04f), 32.0f, 0.0f, 0.4f,
                     cgltf_alpha_mode_opaque, 0.5f, true, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &path, {}, frame, cgltf_alpha_mode_opaque, 0.5f,
                         true);
  expect_true("dummy path URI is unlit paper", ubo.material_flags.x > 0.5f);
  expect_true("dummy path skips COLOR_0 RGB tint",
              ubo.material_flags.z > 0.5f);
  expect_true("dummy path MASK cutout",
              std::fabs(ubo.metallic_roughness_factors.w - 1.0f) < 1e-4f);
  expect_true("dummy path samples albedo", ubo.material_flags.y > 0.5f);
  expect_true("dummy path stays dielectric",
              ubo.metallic_roughness_factors.x < 0.01f);
}

void dummySnowPatchAlbedoUriIsUnlitPaperMask() {
  using namespace Blunder;
  Asset::Meta albedo_meta;
  albedo_meta.virtual_path =
      "resources/se-world/assets/textures/snow_gen_albedo-01.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(albedo_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{235, 248, 255, 255});
  AssetHandle albedo_handle;
  albedo_handle.type = Asset::Type::Texture2D;
  albedo_handle.key = albedo->getVirtualPath();
  Asset::Meta mat_meta;
  mat_meta.virtual_path =
      "assets/Meshes/se-world/SL-hub-snow_patches-m26p0.mesh.yaml#mat";
  MaterialAsset snow(eastl::move(mat_meta), glm::vec4(0.92f, 0.971f, 1.0f, 1.0f),
                     albedo_handle, albedo, nullptr, nullptr, nullptr,
                     glm::vec3(0.15f), glm::vec3(1.0f), glm::vec3(0.04f), 32.0f,
                     0.0f, 0.4f, cgltf_alpha_mode_opaque, 0.5f, true, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &snow, {}, frame, cgltf_alpha_mode_opaque, 0.5f,
                         true);
  expect_true("dummy snow URI is unlit paper", ubo.material_flags.x > 0.5f);
  expect_true("dummy snow skips COLOR_0 RGB tint",
              ubo.material_flags.z > 0.5f);
  expect_true("dummy snow MASK cutout",
              std::fabs(ubo.metallic_roughness_factors.w - 1.0f) < 1e-4f);
  expect_true("dummy snow samples albedo", ubo.material_flags.y > 0.5f);
  expect_true("dummy snow stays dielectric",
              ubo.metallic_roughness_factors.x < 0.01f);
  expect_true("dummy snow keeps albedo_color r",
              std::fabs(ubo.base_color_factor.x - 0.92f) < 1e-4f);
}

void creekAlbedoUriIsUnlitPaperMask() {
  using namespace Blunder;
  Asset::Meta albedo_meta;
  albedo_meta.virtual_path =
      "resources/se-world/assets/lib/textures/creek-bed.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(albedo_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{90, 70, 50, 255});
  AssetHandle albedo_handle;
  albedo_handle.type = Asset::Type::Texture2D;
  albedo_handle.key = albedo->getVirtualPath();
  Asset::Meta mat_meta;
  mat_meta.virtual_path =
      "assets/Meshes/se-world/SL-world-creek-m3p0.mesh.yaml#mat";
  MaterialAsset bed(eastl::move(mat_meta), glm::vec4(1.0f), albedo_handle,
                    albedo, nullptr, nullptr, nullptr, glm::vec3(0.15f),
                    glm::vec3(1.0f), glm::vec3(0.04f), 32.0f, 0.0f, 0.97f,
                    cgltf_alpha_mode_opaque, 0.5f, true, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &bed, {}, frame, cgltf_alpha_mode_opaque, 0.5f,
                         true);
  expect_true("creek bed URI is unlit paper", ubo.material_flags.x > 0.5f);
  expect_true("creek bed skips COLOR_0 RGB tint",
              ubo.material_flags.z > 0.5f);
  expect_true("creek bed MASK cutout",
              std::fabs(ubo.metallic_roughness_factors.w - 1.0f) < 1e-4f);
  expect_true("creek bed samples albedo", ubo.material_flags.y > 0.5f);
  expect_true("creek bed stays dielectric",
              ubo.metallic_roughness_factors.x < 0.01f);
}

void creekWaterFilmIsDielectricBlendNotPaperMask() {
  using namespace Blunder;
  Asset::Meta albedo_meta;
  albedo_meta.virtual_path =
      "resources/se-world/assets/textures/creek_water_surface-albedo.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(albedo_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{40, 90, 140, 255});
  AssetHandle albedo_handle;
  albedo_handle.type = Asset::Type::Texture2D;
  albedo_handle.key = albedo->getVirtualPath();
  Asset::Meta mr_meta;
  mr_meta.virtual_path =
      "resources/se-world/assets/textures/creek_water_surface-roughness.png";
  auto mr = eastl::make_shared<Texture2DAsset>(
      eastl::move(mr_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{0, 128, 255, 255});
  Asset::Meta mat_meta;
  mat_meta.virtual_path =
      "assets/Meshes/se-world/SL-world-creek-m6p0.mesh.yaml#mat";
  MaterialAsset water(eastl::move(mat_meta), glm::vec4(1.0f, 1.0f, 1.0f, 0.4f),
                      albedo_handle, albedo, mr, nullptr, nullptr,
                      glm::vec3(0.15f), glm::vec3(1.0f), glm::vec3(1.0f), 8.0f,
                      1.0f, 1.0f, cgltf_alpha_mode_blend, 0.5f, true, false);
  // Blender extras still ship paper_color on this StandardMaterial3D.
  water.setPaperColor(glm::vec3(1.0f, 1.0f, 1.0f));
  water.promoteWaterSurfaceFilmToOpaque();

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &water, {}, frame, water.getAlphaMode(), 0.5f,
                         true);
  expect_true("creek water stays BLEND over bed paper",
              water.getAlphaMode() == cgltf_alpha_mode_blend);
  expect_true("creek water stays transparent pass",
              water.usesForwardTransparentPass());
  expect_true("creek extras paper_color does not stick", !water.isPaperCard());
  expect_true("creek water is not unlit paper", ubo.material_flags.x < 0.5f);
  expect_true("creek water is not MASK cutout",
              ubo.metallic_roughness_factors.w < 0.5f ||
                  ubo.metallic_roughness_factors.w > 1.5f);
  expect_true("creek water gpu alpha is BLEND",
              std::fabs(ubo.metallic_roughness_factors.w - 2.0f) < 0.1f);
  expect_true("creek water stays dielectric",
              ubo.metallic_roughness_factors.x < 0.01f);
}

void plateauSnowAlbedoUriIsUnlitPaperMask() {
  using namespace Blunder;
  Asset::Meta albedo_meta;
  albedo_meta.virtual_path =
      "resources/se-world/assets/lib/stone_plateau_albedo.png";
  auto albedo = eastl::make_shared<Texture2DAsset>(
      eastl::move(albedo_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{240, 248, 255, 255});
  AssetHandle albedo_handle;
  albedo_handle.type = Asset::Type::Texture2D;
  albedo_handle.key = albedo->getVirtualPath();
  Asset::Meta mat_meta;
  mat_meta.virtual_path =
      "assets/Meshes/se-world/LI-stone_plateau_001-m1p0.mesh.yaml#mat";
  MaterialAsset snow(eastl::move(mat_meta), glm::vec4(1.0f), albedo_handle,
                     albedo, nullptr, nullptr, nullptr, glm::vec3(0.15f),
                     glm::vec3(1.0f), glm::vec3(0.04f), 32.0f, 1.0f, 1.0f,
                     cgltf_alpha_mode_blend, 0.5f, false, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &snow, {}, frame, cgltf_alpha_mode_blend, 0.5f,
                         false);
  expect_true("plateau snow URI is unlit paper", ubo.material_flags.x > 0.5f);
  expect_true("plateau snow skips COLOR_0 RGB tint",
              ubo.material_flags.z > 0.5f);
  expect_true("plateau snow MASK cutout",
              std::fabs(ubo.metallic_roughness_factors.w - 1.0f) < 1e-4f);
  expect_true("plateau snow samples albedo", ubo.material_flags.y > 0.5f);
  expect_true("plateau snow stays dielectric",
              ubo.metallic_roughness_factors.x < 0.01f);
}

void packedOrmMapKeepsMetalChannel() {
  using namespace Blunder;
  Asset::Meta mr_meta;
  mr_meta.virtual_path = "resources/se-world/packed_metallic_roughness.png";
  auto mr = eastl::make_shared<Texture2DAsset>(
      eastl::move(mr_meta), 1u, 1u, 4u,
      eastl::vector<uint8_t>{0, 128, 255, 255});
  Asset::Meta mat_meta;
  mat_meta.virtual_path = "assets/Meshes/helmet.mesh.yaml#mat";
  MaterialAsset chrome(eastl::move(mat_meta), glm::vec4(1.0f), AssetHandle{},
                       nullptr, mr, nullptr, nullptr, glm::vec3(0.0f),
                       glm::vec3(1.0f), glm::vec3(1.0f), 256.0f, 1.0f, 0.2f,
                       cgltf_alpha_mode_opaque, 0.5f, false, false);

  ForwardMeshUniformData ubo{};
  ForwardFrameState frame{};
  frame.live_scene_lighting = true;
  applyPbrToMeshUniforms(ubo, &chrome, {}, frame, cgltf_alpha_mode_opaque, 0.5f,
                         false);
  expect_true("packed ORM keeps metallic factor",
              ubo.metallic_roughness_factors.x > 0.99f);
  expect_true("packed ORM still samples B as metal",
              ubo.material_flags.z < 0.5f);
  expect_true("packed ORM stays opaque",
              ubo.metallic_roughness_factors.w < 0.5f);
  glm::uvec4 bindless{1u, 2u, 3u, 0u};
  applyBindlessPbrMapFlags(ubo.pbr_texture_flags, bindless, ubo.material_flags);
  expect_true("packed ORM still samples tangent normals",
              ubo.pbr_texture_flags.y > 0.5f);
}

void extraMaterialStaysImport() {
  using namespace Blunder;
  Asset::Meta first_meta;
  first_meta.virtual_path = "assets/Meshes/hero.mesh.yaml#mat0";
  auto first = eastl::make_shared<MaterialAsset>(
      eastl::move(first_meta), glm::vec4(0.2f, 0.3f, 0.4f, 1.0f), AssetHandle{},
      nullptr, nullptr, nullptr, nullptr, glm::vec3(0.1f), glm::vec3(0.8f),
      glm::vec3(0.2f), 16.0f, 0.1f, 0.9f, cgltf_alpha_mode_opaque, 0.5f, false,
      false);
  Asset::Meta second_meta;
  second_meta.virtual_path = "assets/Meshes/hero.mesh.yaml#mat1";
  auto second = eastl::make_shared<MaterialAsset>(
      eastl::move(second_meta), glm::vec4(0.5f), AssetHandle{}, nullptr, nullptr,
      nullptr, nullptr, glm::vec3(0.2f), glm::vec3(0.6f), glm::vec3(0.3f), 8.0f,
      0.2f, 0.8f, cgltf_alpha_mode_opaque, 0.5f, false, false);

  MeshMaterialOverride overlay{};
  overlay.unlit.present = true;
  overlay.unlit.value = true;
  overlay.diffuse.present = true;
  overlay.diffuse.value = glm::vec3(1.0f, 0.0f, 0.0f);
  applyMeshMaterialOverride(*first, overlay, {});
  expect_true("first overlay unlit", first->isUnlit());
  expect_true("second stays lit", !second->isUnlit());
  expect_true("second diffuse unchanged", second->getDiffuseColor().x == 0.6f);
}

void assetInspectorRoutesGlobalHistory() {
  using namespace Blunder;
  expect_true("viewport still document",
              resolveUndoScope(false, false, false) ==
                  EditorUndoScope::document);
  expect_true("browser still global",
              resolveUndoScope(true, false, false) == EditorUndoScope::global);
  expect_true("asset inspector global",
              resolveUndoScope(false, false, true) == EditorUndoScope::global);
  expect_true("rename still text",
              resolveUndoScope(true, true, true) == EditorUndoScope::text);
}

void undoFieldAndResetRestoreBag() {
  using namespace Blunder;

  struct BagCommand final : IEditorCommand {
    MeshMaterialOverride* bag{nullptr};
    MeshMaterialOverride before{};
    MeshMaterialOverride after{};
    void undo() override {
      if (bag != nullptr) {
        *bag = before;
      }
    }
    void redo() override {
      if (bag != nullptr) {
        *bag = after;
      }
    }
  };

  DocumentHistory global;
  DocumentHistory document;

  MeshMaterialOverride bag{};
  auto field = eastl::make_unique<BagCommand>();
  field->bag = &bag;
  field->after.shininess.present = true;
  field->after.shininess.value = 48.0f;
  field->redo();
  global.push(eastl::move(field));
  expect_true("field applied",
              bag.shininess.present && bag.shininess.value == 48.0f);

  MeshMaterialOverride scene_bag{};
  auto scene = eastl::make_unique<BagCommand>();
  scene->bag = &scene_bag;
  scene->after.unlit.present = true;
  scene->after.unlit.value = true;
  scene->redo();
  document.push(eastl::move(scene));

  expect_true("asset inspector routes global",
              resolveUndoScope(false, false, true) == EditorUndoScope::global);
  expect_true("global undo field", global.undo());
  expect_true("field bag restored", !bag.shininess.present);
  expect_true("document history untouched", scene_bag.unlit.present);
  expect_true("document still can undo", document.canUndo());

  bag.shininess.present = true;
  bag.shininess.value = 48.0f;
  auto reset = eastl::make_unique<BagCommand>();
  reset->bag = &bag;
  reset->before = bag;
  reset->after = {};
  reset->redo();
  global.push(eastl::move(reset));
  expect_true("reset cleared bag", bag.empty());
  expect_true("undo reset", global.undo());
  expect_true("reset restored shininess",
              bag.shininess.present && bag.shininess.value == 48.0f);
  expect_true("ctrl+z from asset inspector would not pop document",
              document.canUndo() && scene_bag.unlit.present);
}

}  // namespace

int main() {
  overlayScalarsAndClearSlot();
  extraMaterialStaysImport();
  applyBlinnPhongIgnoresEditorBag();
  gltfSpecDefaultMetalWithoutMrMapShadesAsDielectric();
  untexturedMaterialDoesNotSampleAlbedoBindless();
  foliageMrMapWithZeroMetallicStaysDielectric();
  foliageSpecDefaultMetalWithRoughnessAtlasBecomesDielectricAtPack();
  paperGrainNormalIsNotSampledAndPaperColorTints();
  dummyPathAlbedoUriIsUnlitPaperMask();
  dummySnowPatchAlbedoUriIsUnlitPaperMask();
  creekAlbedoUriIsUnlitPaperMask();
  creekWaterFilmIsDielectricBlendNotPaperMask();
  plateauSnowAlbedoUriIsUnlitPaperMask();
  packedOrmMapKeepsMetalChannel();
  assetInspectorRoutesGlobalHistory();
  undoFieldAndResetRestoreBag();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
