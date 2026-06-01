#pragma once

#include <cstdint>

namespace Blunder {

/// A borrowed engine Vulkan image handed to the UI for zero-copy viewport
/// compositing (shared-device path). All Vulkan handles/enums are raw values
/// (reinterpret_cast'd to integers) so the UI layer need not include Vulkan
/// headers. The engine retains ownership of the image and keeps it alive (and
/// in `layout`) until the next present.
struct ViewportVulkanImage {
  uint64_t image{0};   ///< VkImage handle
  uint32_t format{0};  ///< VkFormat (raw)
  uint32_t layout{0};  ///< VkImageLayout the image is sampled in (raw)
  uint32_t width{0};
  uint32_t height{0};
};

}  // namespace Blunder
