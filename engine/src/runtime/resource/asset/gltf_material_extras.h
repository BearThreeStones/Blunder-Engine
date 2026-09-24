#pragma once

#include <cstddef>

namespace Blunder {

/// Godot / blender-studio extras on a glTF material.
struct GltfMaterialExtras {
  bool has_metallic{false};
  float metallic{1.0f};
  bool has_roughness{false};
  float roughness{1.0f};
  bool has_paper_color{false};
  float paper_color[3]{1.0f, 1.0f, 1.0f};
};

/// Reads `material_info.metallic` / `material_info.roughness` /
/// `material_info.paper_color`, then top-level `metallic` / `roughness` /
/// `paper_color`. Does not match `metallicFactor`.
bool parseGltfMaterialExtrasJson(const char* json, size_t length,
                                 GltfMaterialExtras& out);

/// `pine_leaves_roughness_01` is a roughness atlas, not packed ORM (B=metal).
/// Packed names (`metallic_roughness`, `orm`) stay ORM.
bool metallicRoughnessUriIsRoughnessOnly(const char* uri);

/// Godot paper grain (`paper_rough_256`) is exported in the glTF normal slot.
/// It is not a tangent-space normal map.
bool textureUriIsPaperGrain(const char* uri);

/// Blender placeholder `DUMMY-path-*`. Godot remaps these via
/// `material_index.json` / glTF import `_subresources` onto paper ShaderMaterials
/// with dirt albedo PNGs. The exported glTF keeps the dummy 0.8 gray and no
/// texture, so Blunder must bind the Godot map (or a dirt factor) itself.
bool materialNameIsDummyPath(const char* name);

/// Godot `path_1_albedo` / `path_7_albedo` / `path_faint_albedo` / snowman strip.
bool textureUriIsPathPaperAlbedo(const char* uri);

/// `SL-fence-paths` / `SL-hub-paths` / `SL-clearing-path` mesh yaml or glTF.
bool meshSourceLooksLikeDummyPathSet(const char* path);

/// File name under `resources/se-world/assets/lib/textures/`. Null if unknown.
const char* dummyPathAlbedoFileName(const char* material_name);

/// Mean dirt of Godot `path_1_albedo` when the PNG is missing.
void dummyPathFallbackAlbedoRgb(float out[3]);

/// Blender placeholder `DUMMY-snow_patch_*`. Godot remaps these via
/// `SL-*-snow_patches.gltf.import` / `SL-clearing-snow.gltf.import` onto paper
/// ShaderMaterials with `snow_gen_albedo-01.png`. The exported glTF keeps dummy
/// 0.8 gray, no albedo URI, and COLOR_0 G as paper wear (R=B=0, A=1).
bool materialNameIsDummySnowPatch(const char* name);

/// Godot `snow_gen_albedo-01` snow paper.
bool textureUriIsSnowPatchAlbedo(const char* uri);

/// `SL-hub-snow_patches` / `SL-fence-snow_patches` / `SL-clearing-snow`
/// mesh yaml or glTF.
bool meshSourceLooksLikeDummySnowPatchSet(const char* path);

/// File name under `resources/se-world/assets/textures/`. Null if unknown.
const char* dummySnowPatchAlbedoFileName(const char* material_name);

/// Godot `snow_patch_01.tres` `albedo_color` when the PNG is missing.
void dummySnowPatchFallbackAlbedoRgb(float out[3]);

/// Godot `snow_edge_plateau.tres` on `GEO-stone_plateau_snow_*`.
bool materialNameIsSnowEdgePlateau(const char* name);

/// Godot `stone_plateau_albedo` paper on cliff snow overlays.
bool textureUriIsPlateauSnowAlbedo(const char* uri);

/// Godot remaps `creek_bed` / `creek_edge` onto `creek-bed.png` /
/// `creek-snow_edge.png` paper. Water stays a film (`promoteWaterSurfaceFilmToOpaque`).
bool textureUriIsCreekPaperAlbedo(const char* uri);

/// Pond ice (`ice_surface_squiggles`) and creek water (`creek_water_surface`).
/// Not paper cards — extras still carry `paper_color` from the Blender export.
bool textureUriIsWaterSurfaceFilm(const char* uri);

/// `SL-world-creek` mesh yaml or glTF. Empty cooked sidecars must re-hydrate
/// so the Godot albedo URIs bind.
bool meshSourceLooksLikeWorldCreek(const char* path);

/// Extras metallic wins. Else a roughness-only map with spec-default metal 1
/// becomes dielectric so `metallic *= sampledMr.b` cannot chrome the card.
float resolveImportedMetallicFactor(float gltf_metallic_factor,
                                    const GltfMaterialExtras& extras,
                                    const char* metallic_roughness_uri);

float resolveImportedRoughnessFactor(float gltf_roughness_factor,
                                     const GltfMaterialExtras& extras);

}  // namespace Blunder
