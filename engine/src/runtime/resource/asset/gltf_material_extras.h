#pragma once

#include <cstddef>

namespace Blunder {

/// Godot / blender-studio extras on a glTF material. `paper_color` is parsed
/// nowhere yet — the paper shader is still missing.
struct GltfMaterialExtras {
  bool has_metallic{false};
  float metallic{1.0f};
  bool has_roughness{false};
  float roughness{1.0f};
};

/// Reads `material_info.metallic` / `material_info.roughness`, then top-level
/// `metallic` / `roughness`. Does not match `metallicFactor`.
bool parseGltfMaterialExtrasJson(const char* json, size_t length,
                                 GltfMaterialExtras& out);

/// `pine_leaves_roughness_01` is a roughness atlas, not packed ORM (B=metal).
/// Packed names (`metallic_roughness`, `orm`) stay ORM.
bool metallicRoughnessUriIsRoughnessOnly(const char* uri);

/// Extras metallic wins. Else a roughness-only map with spec-default metal 1
/// becomes dielectric so `metallic *= sampledMr.b` cannot chrome the card.
float resolveImportedMetallicFactor(float gltf_metallic_factor,
                                    const GltfMaterialExtras& extras,
                                    const char* metallic_roughness_uri);

float resolveImportedRoughnessFactor(float gltf_roughness_factor,
                                     const GltfMaterialExtras& extras);

}  // namespace Blunder
