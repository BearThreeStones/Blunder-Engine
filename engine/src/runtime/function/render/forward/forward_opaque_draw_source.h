#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

#include "EASTL/shared_ptr.h"
#include "EASTL/unique_ptr.h"

namespace Blunder {

class MaterialAsset;
class VulkanBuffer;
class VulkanTexture;

/// CPU-side mesh resources registered with the forward path (one descriptor slot).
struct ForwardOpaqueDrawSource {
  eastl::unique_ptr<VulkanBuffer> vertex_buffer;
  eastl::unique_ptr<VulkanBuffer> index_buffer;
  uint32_t index_count{0};
  uint32_t slot_index{0};
  eastl::shared_ptr<MaterialAsset> material;
  VulkanTexture* base_color_texture{nullptr};

  glm::vec3 translation{0.0f};
  glm::vec3 scale{1.0f};
  float rotation_z{0.0f};
  bool animate_spin{false};
};

}  // namespace Blunder
