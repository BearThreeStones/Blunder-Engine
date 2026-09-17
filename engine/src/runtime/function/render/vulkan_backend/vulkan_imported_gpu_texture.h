#pragma once

#include <vulkan/vulkan.h>

#include "runtime/function/render/rhi/i_gpu_texture.h"

namespace Blunder::vulkan_backend {

/// Non-owning External import for Frame graph. Does not create or free the
/// image. Viewport Scene uses this for offscreen color/depth and the shadow
/// map. Decision: docs/adr/0067-frame-graph-viewport-wire.md.
class VulkanImportedGpuTexture final : public rhi::IGpuTexture {
 public:
  void bind(VkImage image, VkImageAspectFlags aspect);

  VkImage vkImage() const { return m_image; }
  VkImageAspectFlags aspectMask() const { return m_aspect; }

 private:
  VkImage m_image{VK_NULL_HANDLE};
  VkImageAspectFlags m_aspect{VK_IMAGE_ASPECT_COLOR_BIT};
};

}  // namespace Blunder::vulkan_backend
