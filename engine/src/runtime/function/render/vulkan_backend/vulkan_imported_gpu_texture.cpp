#include "runtime/function/render/vulkan_backend/vulkan_imported_gpu_texture.h"

namespace Blunder::vulkan_backend {

void VulkanImportedGpuTexture::bind(VkImage image, VkImageAspectFlags aspect) {
  m_image = image;
  m_aspect = aspect;
}

}  // namespace Blunder::vulkan_backend
