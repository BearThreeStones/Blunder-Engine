#include "runtime/function/render/offscreen_render_target.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

void OffscreenRenderTarget::initialize(VulkanContext* context,
                                       VulkanAllocator* allocator,
                                       uint32_t width, uint32_t height,
                                       VkFormat format) {
  ASSERT(context);
  ASSERT(allocator);
  ASSERT(width > 0 && height > 0);

  m_context = context;
  m_allocator = allocator;
  m_format = format;
  m_width = width;
  m_height = height;

  createRenderPass();
  createImageAndFramebuffer();
}

void OffscreenRenderTarget::shutdown() {
  if (!m_context) {
    return;
  }

  destroyImageAndFramebuffer();

  if (m_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_context->getDevice(), m_render_pass, nullptr);
    m_render_pass = VK_NULL_HANDLE;
  }

  m_context = nullptr;
  m_allocator = nullptr;
  m_width = 0;
  m_height = 0;
}

void OffscreenRenderTarget::resize(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return;
  }
  if (width == m_width && height == m_height) {
    return;
  }
  m_width = width;
  m_height = height;
  destroyImageAndFramebuffer();
  createImageAndFramebuffer();
}

void OffscreenRenderTarget::createRenderPass() {
  VkAttachmentDescription color_attachment{};
  color_attachment.format = m_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference color_attachment_ref{};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;

  VkSubpassDependency dependencies[2]{};
  // External -> 0: ensure previous fragment shader read finishes before we
  // start writing the color attachment again.
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  // 0 -> External: make color writes available to subsequent fragment shader
  // reads (UI overlay) or transfer reads (CPU readback).
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_TRANSFER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask =
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 2;
  render_pass_info.pDependencies = dependencies;

  const VkResult result = vkCreateRenderPass(
      m_context->getDevice(), &render_pass_info, nullptr, &m_render_pass);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createRenderPass] vkCreateRenderPass failed: "
        "{}",
        static_cast<int>(result));
  }
}

void OffscreenRenderTarget::createImageAndFramebuffer() {
  ASSERT(m_render_pass != VK_NULL_HANDLE);
  ASSERT(m_width > 0 && m_height > 0);

  VkDevice device = m_context->getDevice();

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
  image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_create{};
  alloc_create.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  const VkResult img_result = vmaCreateImage(
      m_allocator->getAllocator(), &image_info, &alloc_create, &m_image,
      &m_image_allocation, nullptr);
  if (img_result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createImageAndFramebuffer] vmaCreateImage "
        "failed: {}",
        static_cast<int>(img_result));
  }

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = m_format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  const VkResult view_result =
      vkCreateImageView(device, &view_info, nullptr, &m_image_view);
  if (view_result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createImageAndFramebuffer] vkCreateImageView "
        "failed: {}",
        static_cast<int>(view_result));
  }

  VkFramebufferCreateInfo fb_info{};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.renderPass = m_render_pass;
  fb_info.attachmentCount = 1;
  fb_info.pAttachments = &m_image_view;
  fb_info.width = m_width;
  fb_info.height = m_height;
  fb_info.layers = 1;

  const VkResult fb_result =
      vkCreateFramebuffer(device, &fb_info, nullptr, &m_framebuffer);
  if (fb_result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createImageAndFramebuffer] "
        "vkCreateFramebuffer failed: {}",
        static_cast<int>(fb_result));
  }

  m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void OffscreenRenderTarget::destroyImageAndFramebuffer() {
  if (!m_context) {
    return;
  }

  VkDevice device = m_context->getDevice();
  if (m_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, m_framebuffer, nullptr);
    m_framebuffer = VK_NULL_HANDLE;
  }
  if (m_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, m_image_view, nullptr);
    m_image_view = VK_NULL_HANDLE;
  }
  if (m_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), m_image, m_image_allocation);
    m_image = VK_NULL_HANDLE;
    m_image_allocation = VK_NULL_HANDLE;
  }
  m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void OffscreenRenderTarget::cmdBarrierToTransferSrc(VkCommandBuffer cmd) {
  if (m_image == VK_NULL_HANDLE) {
    return;
  }

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = m_current_layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask =
      m_current_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
          ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
          : VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

  const VkPipelineStageFlags src_stage =
      m_current_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
          ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
          : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  vkCmdPipelineBarrier(cmd, src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  m_current_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
}

void OffscreenRenderTarget::cmdBarrierToShaderRead(VkCommandBuffer cmd) {
  if (m_image == VK_NULL_HANDLE) {
    return;
  }
  if (m_current_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    return;
  }

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = m_current_layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask =
      m_current_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
          ? VK_ACCESS_TRANSFER_READ_BIT
          : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  const VkPipelineStageFlags src_stage =
      m_current_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
          ? VK_PIPELINE_STAGE_TRANSFER_BIT
          : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  vkCmdPipelineBarrier(cmd, src_stage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                       0, nullptr, 0, nullptr, 1, &barrier);

  m_current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

}  // namespace Blunder
