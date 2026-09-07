#include "runtime/function/render/vulkan/vulkan_texture.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

eastl::string gpuTextureCacheKey(const Texture2DAsset& asset) {
  eastl::string key = asset.getVirtualPath();
  if (!key.empty()) {
    return key;
  }
  const std::filesystem::path& absolute_path = asset.getAbsolutePath();
  if (!absolute_path.empty()) {
    return eastl::string(absolute_path.generic_string().c_str());
  }
  return eastl::string("generated://gpu-texture/anonymous");
}

void VulkanTexture::createFromTexture2DAsset(VulkanContext* context,
                                             VulkanAllocator* allocator,
                                             const Texture2DAsset& asset) {
  ASSERT(context);
  ASSERT(allocator);
  ASSERT(asset.getChannels() == 4);

  destroy();

  const uint32_t width = asset.getWidth();
  const uint32_t height = asset.getHeight();
  ASSERT(width > 0 && height > 0);

  m_context = context;
  m_allocator = allocator;

  m_image.create(m_context, m_allocator, width, height,
                 VK_FORMAT_R8G8B8A8_UNORM,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
  m_image.uploadTexture2D(asset);
}

void VulkanTexture::destroy() {
  if (m_context != nullptr) {
    m_context->bindlessTextureTable().release(this);
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VulkanAllocator* allocator = nullptr;
    if (m_image.releaseGpuHandles(image, view, sampler, allocation,
                                  allocator)) {
      m_context->retireSampledImage(image, view, sampler, allocation,
                                    allocator);
    }
  } else {
    m_image.destroy();
  }
  m_allocator = nullptr;
  m_context = nullptr;
}

}  // namespace Blunder