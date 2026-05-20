#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>

#include <cgltf.h>

#include "EASTL/shared_ptr.h"

namespace Blunder {

class GpuMesh;
class MaterialAsset;
class VulkanTexture;

/// Registered mesh draw (GPU mesh shared; per-instance model matrix).
struct OpaqueMeshDraw {
  GpuMesh* gpu_mesh{nullptr};
  eastl::shared_ptr<MaterialAsset> material;
  VulkanTexture* base_color_texture{nullptr};
  VulkanTexture* metallic_roughness_texture{nullptr};
  VulkanTexture* normal_texture{nullptr};
  VulkanTexture* occlusion_texture{nullptr};
  glm::mat4 model{1.0f};
  uint32_t slot_index{0};
  float alpha_cutoff{0.5f};
  cgltf_alpha_mode alpha_mode{cgltf_alpha_mode_opaque};
  bool double_sided{false};
  bool is_transparent{false};
  float sort_depth{0.0f};
};

}  // namespace Blunder
