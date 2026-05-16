#include "runtime/function/render/vulkan_backend/vulkan_gpu_texture.h"

#include "runtime/function/render/vulkan/vulkan_texture.h"

namespace Blunder::vulkan_backend {

void VulkanGpuTexture::setTexture(eastl::unique_ptr<VulkanTexture> texture) {
  m_texture = eastl::move(texture);
}

}  // namespace Blunder::vulkan_backend
