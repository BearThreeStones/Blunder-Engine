#include "runtime/function/render/shadow/shadow_map_target.h"

#include <vk_mem_alloc.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

void ShadowMapTarget::initialize(VulkanContext* context,
                                 VulkanAllocator* allocator) {
  ASSERT(context);
  ASSERT(allocator);

  m_context = context;
  m_allocator = allocator;

  createRenderPass();
  createResources();
}

void ShadowMapTarget::shutdown() {
  if (!m_context) {
    return;
  }

  destroyResources();

  if (m_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_context->getDevice(), m_render_pass, nullptr);
    m_render_pass = VK_NULL_HANDLE;
  }

  m_context = nullptr;
  m_allocator = nullptr;
}

void ShadowMapTarget::createRenderPass() {
  VkAttachmentDescription depth_attachment{};
  depth_attachment.format = VK_FORMAT_D32_SFLOAT;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference depth_ref{};
  depth_ref.attachment = 0;
  depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pDepthStencilAttachment = &depth_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &depth_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  const VkResult result = vkCreateRenderPass(m_context->getDevice(),
                                             &render_pass_info, nullptr,
                                             &m_render_pass);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[ShadowMapTarget] vkCreateRenderPass failed: {}",
              static_cast<int>(result));
  }
}

void ShadowMapTarget::createResources() {
  ASSERT(m_render_pass != VK_NULL_HANDLE);

  VkDevice device = m_context->getDevice();

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = VK_FORMAT_D32_SFLOAT;
  image_info.extent.width = k_shadow_map_size;
  image_info.extent.height = k_shadow_map_size;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  const VkResult img_result =
      vmaCreateImage(m_allocator->getAllocator(), &image_info, &alloc_info,
                     &m_depth_image, &m_depth_allocation, nullptr);
  if (img_result != VK_SUCCESS) {
    LOG_FATAL("[ShadowMapTarget] vmaCreateImage failed: {}",
              static_cast<int>(img_result));
  }

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_depth_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = VK_FORMAT_D32_SFLOAT;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  const VkResult view_result =
      vkCreateImageView(device, &view_info, nullptr, &m_depth_image_view);
  if (view_result != VK_SUCCESS) {
    LOG_FATAL("[ShadowMapTarget] vkCreateImageView failed: {}",
              static_cast<int>(view_result));
  }

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.compareEnable = VK_TRUE;
  sampler_info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  sampler_info.unnormalizedCoordinates = VK_FALSE;

  const VkResult sampler_result =
      vkCreateSampler(device, &sampler_info, nullptr, &m_comparison_sampler);
  if (sampler_result != VK_SUCCESS) {
    LOG_FATAL("[ShadowMapTarget] vkCreateSampler failed: {}",
              static_cast<int>(sampler_result));
  }

  VkImageView attachments[] = {m_depth_image_view};
  VkFramebufferCreateInfo fb_info{};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.renderPass = m_render_pass;
  fb_info.attachmentCount = 1;
  fb_info.pAttachments = attachments;
  fb_info.width = k_shadow_map_size;
  fb_info.height = k_shadow_map_size;
  fb_info.layers = 1;

  const VkResult fb_result =
      vkCreateFramebuffer(device, &fb_info, nullptr, &m_framebuffer);
  if (fb_result != VK_SUCCESS) {
    LOG_FATAL("[ShadowMapTarget] vkCreateFramebuffer failed: {}",
              static_cast<int>(fb_result));
  }

  m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void ShadowMapTarget::destroyResources() {
  if (!m_context) {
    return;
  }

  VkDevice device = m_context->getDevice();

  if (m_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, m_framebuffer, nullptr);
    m_framebuffer = VK_NULL_HANDLE;
  }
  if (m_comparison_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_comparison_sampler, nullptr);
    m_comparison_sampler = VK_NULL_HANDLE;
  }
  if (m_depth_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, m_depth_image_view, nullptr);
    m_depth_image_view = VK_NULL_HANDLE;
  }
  if (m_depth_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), m_depth_image,
                    m_depth_allocation);
    m_depth_image = VK_NULL_HANDLE;
    m_depth_allocation = VK_NULL_HANDLE;
  }

  m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void ShadowMapTarget::beginRenderPass(VkCommandBuffer cmd, float depth_clear) {
  ASSERT(m_framebuffer != VK_NULL_HANDLE);

  if (m_current_layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
      m_current_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_current_layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_depth_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);
  }

  VkClearValue clear_value{};
  clear_value.depthStencil = {depth_clear, 0};

  VkRenderPassBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin_info.renderPass = m_render_pass;
  begin_info.framebuffer = m_framebuffer;
  begin_info.renderArea.offset = {0, 0};
  begin_info.renderArea.extent = getExtent();
  begin_info.clearValueCount = 1;
  begin_info.pClearValues = &clear_value;

  vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
  m_current_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void ShadowMapTarget::endRenderPass(VkCommandBuffer cmd) {
  vkCmdEndRenderPass(cmd);
  m_current_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
}

void ShadowMapTarget::cmdBarrierToShaderReadDepth(VkCommandBuffer cmd) {
  if (m_depth_image == VK_NULL_HANDLE) {
    return;
  }
  if (m_current_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
    return;
  }

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = m_current_layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_depth_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd,
                       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  m_current_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
}

}  // namespace Blunder
