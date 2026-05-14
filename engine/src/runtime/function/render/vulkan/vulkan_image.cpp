#include "runtime/function/render/vulkan/vulkan_image.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

namespace {

struct BarrierInfo {
  VkAccessFlags src_access_mask{0};
  VkAccessFlags dst_access_mask{0};
  VkPipelineStageFlags src_stage_mask{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
  VkPipelineStageFlags dst_stage_mask{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
};

BarrierInfo buildBarrierInfo(VkImageLayout old_layout,
                             VkImageLayout new_layout) {
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    return {0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT};
  }

  if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
      new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    return {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
  }

  if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    return {VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT};
  }

  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    return {0, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
  }

  LOG_FATAL("[VulkanImage] unsupported layout transition: {} -> {}",
            static_cast<int>(old_layout), static_cast<int>(new_layout));
  return {};
}

}  // namespace

void VulkanImage::create(VulkanContext* context, VulkanAllocator* allocator,
                         uint32_t width, uint32_t height, VkFormat format,
                         VkImageUsageFlags usage,
                         VkImageAspectFlags aspect_mask) {
  ASSERT(context);
  ASSERT(allocator);
  ASSERT(width > 0 && height > 0);
  ASSERT(format != VK_FORMAT_UNDEFINED);

  destroy();

  m_context = context;
  m_allocator = allocator;

  m_width = width;
  m_height = height;
  m_format = format;
  m_usage = usage;
  m_aspect_mask = aspect_mask;

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = m_format;
  image_info.extent.width = m_width;
  image_info.extent.height = m_height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = m_usage;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  const VkResult image_result =
      vmaCreateImage(m_allocator->getAllocator(), &image_info, &alloc_info,
                     &m_image, &m_allocation, nullptr);
  if (image_result != VK_SUCCESS) {
    LOG_FATAL("[VulkanImage::create] vmaCreateImage failed: {}",
              static_cast<int>(image_result));
  }

  createImageView();
  if ((m_usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0 &&
      (m_aspect_mask & VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
    createSampler();
  }

  m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void VulkanImage::destroy() {
  if (!m_context) {
    return;
  }

  VkDevice device = m_context->getDevice();
  if (m_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_sampler, nullptr);
    m_sampler = VK_NULL_HANDLE;
  }
  if (m_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, m_image_view, nullptr);
    m_image_view = VK_NULL_HANDLE;
  }
  if (m_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), m_image, m_allocation);
    m_image = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
  }

  m_context = nullptr;
  m_allocator = nullptr;
  m_width = 0;
  m_height = 0;
  m_format = VK_FORMAT_UNDEFINED;
  m_usage = 0;
  m_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
  m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void VulkanImage::uploadTexture2D(const Texture2DAsset& texture) {
  ASSERT(texture.getChannels() == 4u);
  uploadPixels(texture.getPixelData(), texture.getPixelByteSize(),
               texture.getWidth(), texture.getHeight());
}

void VulkanImage::cmdTransitionLayout(VkCommandBuffer command_buffer,
                                      VkImageLayout new_layout) {
  ASSERT(command_buffer != VK_NULL_HANDLE);

  if (m_image == VK_NULL_HANDLE || m_current_layout == new_layout) {
    return;
  }

  const BarrierInfo info = buildBarrierInfo(m_current_layout, new_layout);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = m_current_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_image;
  barrier.subresourceRange.aspectMask = m_aspect_mask;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = info.src_access_mask;
  barrier.dstAccessMask = info.dst_access_mask;

  vkCmdPipelineBarrier(command_buffer, info.src_stage_mask,
                       info.dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);

  m_current_layout = new_layout;
}

void VulkanImage::createImageView() {
  ASSERT(m_context);
  ASSERT(m_image != VK_NULL_HANDLE);

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = m_format;
  view_info.subresourceRange.aspectMask = m_aspect_mask;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  const VkResult view_result =
      vkCreateImageView(m_context->getDevice(), &view_info, nullptr,
                        &m_image_view);
  if (view_result != VK_SUCCESS) {
    LOG_FATAL("[VulkanImage::createImageView] vkCreateImageView failed: {}",
              static_cast<int>(view_result));
  }
}

void VulkanImage::createSampler() {
  ASSERT(m_context);

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.anisotropyEnable =
      m_context->isSamplerAnisotropyEnabled() ? VK_TRUE : VK_FALSE;
  sampler_info.maxAnisotropy = m_context->isSamplerAnisotropyEnabled()
                                   ? m_context->getMaxSamplerAnisotropy()
                                   : 1.0f;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_info.mipLodBias = 0.0f;
  sampler_info.minLod = 0.0f;
  sampler_info.maxLod = 0.0f;

  const VkResult sampler_result =
      vkCreateSampler(m_context->getDevice(), &sampler_info, nullptr,
                      &m_sampler);
  if (sampler_result != VK_SUCCESS) {
    LOG_FATAL("[VulkanImage::createSampler] vkCreateSampler failed: {}",
              static_cast<int>(sampler_result));
  }
}

void VulkanImage::uploadPixels(const void* pixel_data, size_t pixel_byte_size,
                               uint32_t width, uint32_t height) {
  ASSERT(m_context);
  ASSERT(m_allocator);
  ASSERT(m_image != VK_NULL_HANDLE);
  ASSERT(pixel_data);
  ASSERT(pixel_byte_size > 0);
  ASSERT(width == m_width && height == m_height);
  ASSERT((m_usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0);

  VulkanBuffer staging_buffer;
  staging_buffer.create(m_allocator, static_cast<VkDeviceSize>(pixel_byte_size),
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VMA_MEMORY_USAGE_CPU_TO_GPU);
  staging_buffer.upload(pixel_data, static_cast<VkDeviceSize>(pixel_byte_size));

  VkCommandBuffer command_buffer = m_context->beginImmediateCommands();
  cmdTransitionLayout(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkBufferImageCopy copy_region{};
  copy_region.bufferOffset = 0;
  copy_region.bufferRowLength = 0;
  copy_region.bufferImageHeight = 0;
  copy_region.imageSubresource.aspectMask = m_aspect_mask;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageOffset = {0, 0, 0};
  copy_region.imageExtent.width = width;
  copy_region.imageExtent.height = height;
  copy_region.imageExtent.depth = 1;

  vkCmdCopyBufferToImage(command_buffer, staging_buffer.getBuffer(), m_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &copy_region);

  cmdTransitionLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_context->endImmediateCommands(command_buffer);

  staging_buffer.destroy();
}

}  // namespace Blunder