#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>

#include <cgltf.h>

namespace Blunder {

class MaterialAsset;
class VulkanBuffer;
class VulkanTexture;

/// One indexed mesh draw submitted during the forward pass.
struct ForwardOpaqueDraw {
  uint32_t slot_index{0};
  VulkanBuffer* vertex_buffer{nullptr};
  VulkanBuffer* index_buffer{nullptr};
  uint32_t index_count{0};
  glm::mat4 model{1.0f};
  glm::mat4 normal_matrix{1.0f};
  const MaterialAsset* material{nullptr};
  VulkanTexture* base_color_texture{nullptr};
  VulkanTexture* metallic_roughness_texture{nullptr};
  VulkanTexture* normal_texture{nullptr};
  VulkanTexture* occlusion_texture{nullptr};
  float alpha_cutoff{0.5f};
  cgltf_alpha_mode alpha_mode{cgltf_alpha_mode_opaque};
  bool double_sided{false};
};

}  // namespace Blunder
