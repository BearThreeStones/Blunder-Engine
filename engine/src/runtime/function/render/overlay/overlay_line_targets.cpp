#include "runtime/function/render/overlay/overlay_line_targets.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

namespace {

void createColorImage(VulkanContext* context, VulkanAllocator* allocator,
                      uint32_t width, uint32_t height, VkFormat format,
                      VkImageUsageFlags usage, VkImage* out_image,
                      VmaAllocation* out_allocation, VkImageView* out_view) {
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent = {width, height, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = format;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = usage;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  vmaCreateImage(allocator->getAllocator(), &image_info, &alloc_info,
                 out_image, out_allocation, nullptr);

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = *out_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;
  vkCreateImageView(context->getDevice(), &view_info, nullptr, out_view);
}

}  // namespace

OverlayLineTargets::~OverlayLineTargets() {
  shutdown();
}

void OverlayLineTargets::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                                    OffscreenRenderTarget* offscreen) {
  ASSERT(ctx);
  ASSERT(alloc);
  ASSERT(offscreen);
  m_context = ctx;
  m_allocator = alloc;
  m_offscreen = offscreen;
  const VkExtent2D extent = offscreen->getExtent();
  resize(extent.width, extent.height);
}

void OverlayLineTargets::shutdown() {
  destroyImages();
  m_offscreen = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
}

void OverlayLineTargets::resize(uint32_t width, uint32_t height) {
  if (m_width == width && m_height == height && m_framebuffer != VK_NULL_HANDLE) {
    return;
  }
  destroyImages();
  if (width == 0 || height == 0 || m_context == nullptr) {
    return;
  }

  m_width = width;
  m_height = height;

  const VkImageUsageFlags color_usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

  createColorImage(m_context, m_allocator, width, height,
                   VK_FORMAT_R8G8B8A8_UNORM, color_usage,
                   &m_overlay_color_image, &m_overlay_color_allocation,
                   &m_overlay_color_view);
  m_overlay_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;

  createColorImage(m_context, m_allocator, width, height,
                   VK_FORMAT_R16G16B16A16_UNORM, color_usage,
                   &m_line_data_image, &m_line_data_allocation, &m_line_data_view);
  m_line_data_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void OverlayLineTargets::recreateFramebuffer(VkRenderPass render_pass) {
  if (m_context == nullptr || render_pass == VK_NULL_HANDLE ||
      m_width == 0 || m_height == 0) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, m_framebuffer, nullptr);
    m_framebuffer = VK_NULL_HANDLE;
  }

  VkImageView attachments[] = {m_overlay_color_view, m_line_data_view,
                                 m_offscreen->getDepthImageView()};
  VkFramebufferCreateInfo fb_info{};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.renderPass = render_pass;
  fb_info.attachmentCount = 3;
  fb_info.pAttachments = attachments;
  fb_info.width = m_width;
  fb_info.height = m_height;
  fb_info.layers = 1;
  const VkResult result =
      vkCreateFramebuffer(device, &fb_info, nullptr, &m_framebuffer);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[OverlayLineTargets] vkCreateFramebuffer failed: {}",
              static_cast<int>(result));
  }
}

void OverlayLineTargets::destroyImages() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();

  if (m_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, m_framebuffer, nullptr);
    m_framebuffer = VK_NULL_HANDLE;
  }

  auto destroy_view_image = [&](VkImageView& view, VkImage& image,
                                VmaAllocation& alloc) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, view, nullptr);
      view = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE) {
      vmaDestroyImage(m_allocator->getAllocator(), image, alloc);
      image = VK_NULL_HANDLE;
      alloc = VK_NULL_HANDLE;
    }
  };

  destroy_view_image(m_overlay_color_view, m_overlay_color_image,
                     m_overlay_color_allocation);
  destroy_view_image(m_line_data_view, m_line_data_image,
                     m_line_data_allocation);

  m_overlay_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  m_line_data_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  m_width = 0;
  m_height = 0;
}

VkExtent2D OverlayLineTargets::extent() const {
  return {m_width, m_height};
}

void OverlayLineTargets::cmdBarrierOverlayTargetsToShaderRead(
    VkCommandBuffer cmd) {
  auto barrier = [&](VkImage image, VkImageLayout& tracked) {
    if (image == VK_NULL_HANDLE ||
        tracked == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      return;
    }
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = tracked;
    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &b);
    tracked = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  };
  barrier(m_overlay_color_image, m_overlay_color_layout);
  barrier(m_line_data_image, m_line_data_layout);
}

}  // namespace Blunder
