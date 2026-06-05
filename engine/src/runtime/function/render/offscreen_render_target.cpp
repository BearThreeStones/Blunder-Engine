#include "runtime/function/render/offscreen_render_target.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

namespace {

void checkBufferIndex(uint32_t index) {
  ASSERT(index < OffscreenRenderTarget::k_buffer_count);
}

}  // namespace

const OffscreenRenderTarget::BufferSlot& OffscreenRenderTarget::activeSlot() const {
  checkBufferIndex(m_active_buffer_index);
  return m_buffers[m_active_buffer_index];
}

OffscreenRenderTarget::BufferSlot& OffscreenRenderTarget::activeSlot() {
  checkBufferIndex(m_active_buffer_index);
  return m_buffers[m_active_buffer_index];
}

void OffscreenRenderTarget::setActiveBufferIndex(const uint32_t index) {
  checkBufferIndex(index);
  m_active_buffer_index = index;
}

VkImage OffscreenRenderTarget::getImage() const {
  return activeSlot().color_image;
}

VkImage OffscreenRenderTarget::getImage(const uint32_t buffer_index) const {
  checkBufferIndex(buffer_index);
  return m_buffers[buffer_index].color_image;
}

VkImageView OffscreenRenderTarget::getImageView() const {
  return activeSlot().color_view;
}

VkImage OffscreenRenderTarget::getDepthImage() const {
  return activeSlot().depth_image;
}

VkImageView OffscreenRenderTarget::getDepthImageView() const {
  return activeSlot().depth_view;
}

VkFramebuffer OffscreenRenderTarget::getFramebuffer() const {
  return activeSlot().framebuffer;
}

VkImageLayout OffscreenRenderTarget::getCurrentLayout() const {
  return activeSlot().color_layout;
}

void OffscreenRenderTarget::setCurrentLayout(const VkImageLayout layout) {
  activeSlot().color_layout = layout;
}

VkImageLayout OffscreenRenderTarget::getDepthLayout() const {
  return activeSlot().depth_layout;
}

void OffscreenRenderTarget::setDepthLayout(const VkImageLayout layout) {
  activeSlot().depth_layout = layout;
}

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
  m_active_buffer_index = 0;

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

  VkAttachmentDescription depth_attachment{};
  depth_attachment.format = VK_FORMAT_D32_SFLOAT;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference depth_attachment_ref{};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkSubpassDependency dependencies[2]{};
  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
  dependencies[1].srcSubpass = 0;
  dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_TRANSFER_BIT;
  dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[1].dstAccessMask =
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
  dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  VkAttachmentDescription attachments[2] = {color_attachment, depth_attachment};
  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 2;
  render_pass_info.pAttachments = attachments;
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

void OffscreenRenderTarget::createBufferSlot(const uint32_t slot_index) {
  checkBufferIndex(slot_index);
  ASSERT(m_render_pass != VK_NULL_HANDLE);
  ASSERT(m_width > 0 && m_height > 0);

  BufferSlot& slot = m_buffers[slot_index];
  VkDevice device = m_context->getDevice();

  VkImageCreateInfo color_info{};
  color_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  color_info.imageType = VK_IMAGE_TYPE_2D;
  color_info.format = m_format;
  color_info.extent = {m_width, m_height, 1};
  color_info.mipLevels = 1;
  color_info.arrayLayers = 1;
  color_info.samples = VK_SAMPLE_COUNT_1_BIT;
  color_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  color_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  color_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_create{};
  alloc_create.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VkResult result = vmaCreateImage(m_allocator->getAllocator(), &color_info,
                                   &alloc_create, &slot.color_image,
                                   &slot.color_allocation, nullptr);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createBufferSlot] color vmaCreateImage "
        "failed (slot {}): {}",
        slot_index, static_cast<int>(result));
  }

  VkImageViewCreateInfo color_view_info{};
  color_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  color_view_info.image = slot.color_image;
  color_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  color_view_info.format = m_format;
  color_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  color_view_info.subresourceRange.levelCount = 1;
  color_view_info.subresourceRange.layerCount = 1;

  result = vkCreateImageView(device, &color_view_info, nullptr,
                             &slot.color_view);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createBufferSlot] color vkCreateImageView "
        "failed (slot {}): {}",
        slot_index, static_cast<int>(result));
  }

  VkImageCreateInfo depth_info{};
  depth_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  depth_info.imageType = VK_IMAGE_TYPE_2D;
  depth_info.format = VK_FORMAT_D32_SFLOAT;
  depth_info.extent = {m_width, m_height, 1};
  depth_info.mipLevels = 1;
  depth_info.arrayLayers = 1;
  depth_info.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  depth_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT;
  depth_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  result = vmaCreateImage(m_allocator->getAllocator(), &depth_info, &alloc_create,
                          &slot.depth_image, &slot.depth_allocation, nullptr);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createBufferSlot] depth vmaCreateImage "
        "failed (slot {}): {}",
        slot_index, static_cast<int>(result));
  }

  VkImageViewCreateInfo depth_view_info{};
  depth_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depth_view_info.image = slot.depth_image;
  depth_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depth_view_info.format = VK_FORMAT_D32_SFLOAT;
  depth_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  depth_view_info.subresourceRange.levelCount = 1;
  depth_view_info.subresourceRange.layerCount = 1;

  result = vkCreateImageView(device, &depth_view_info, nullptr, &slot.depth_view);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createBufferSlot] depth vkCreateImageView "
        "failed (slot {}): {}",
        slot_index, static_cast<int>(result));
  }

  VkImageView attachments[2] = {slot.color_view, slot.depth_view};
  VkFramebufferCreateInfo fb_info{};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.renderPass = m_render_pass;
  fb_info.attachmentCount = 2;
  fb_info.pAttachments = attachments;
  fb_info.width = m_width;
  fb_info.height = m_height;
  fb_info.layers = 1;

  result = vkCreateFramebuffer(device, &fb_info, nullptr, &slot.framebuffer);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[OffscreenRenderTarget::createBufferSlot] vkCreateFramebuffer failed "
        "(slot {}): {}",
        slot_index, static_cast<int>(result));
  }

  slot.color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  slot.depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void OffscreenRenderTarget::destroyBufferSlot(const uint32_t slot_index) {
  if (!m_context) {
    return;
  }
  checkBufferIndex(slot_index);

  BufferSlot& slot = m_buffers[slot_index];
  VkDevice device = m_context->getDevice();

  if (slot.framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, slot.framebuffer, nullptr);
    slot.framebuffer = VK_NULL_HANDLE;
  }
  if (slot.color_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, slot.color_view, nullptr);
    slot.color_view = VK_NULL_HANDLE;
  }
  if (slot.depth_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, slot.depth_view, nullptr);
    slot.depth_view = VK_NULL_HANDLE;
  }
  if (slot.color_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), slot.color_image,
                    slot.color_allocation);
    slot.color_image = VK_NULL_HANDLE;
    slot.color_allocation = VK_NULL_HANDLE;
  }
  if (slot.depth_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), slot.depth_image,
                    slot.depth_allocation);
    slot.depth_image = VK_NULL_HANDLE;
    slot.depth_allocation = VK_NULL_HANDLE;
  }
  slot.color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  slot.depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void OffscreenRenderTarget::createImageAndFramebuffer() {
  for (uint32_t slot = 0; slot < k_buffer_count; ++slot) {
    createBufferSlot(slot);
  }
}

void OffscreenRenderTarget::destroyImageAndFramebuffer() {
  for (uint32_t slot = 0; slot < k_buffer_count; ++slot) {
    destroyBufferSlot(slot);
  }
}

void OffscreenRenderTarget::cmdBarrierToTransferSrc(VkCommandBuffer cmd) {
  BufferSlot& slot = activeSlot();
  if (slot.color_image == VK_NULL_HANDLE) {
    return;
  }

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = slot.color_layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = slot.color_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask =
      slot.color_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
          ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
          : VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

  const VkPipelineStageFlags src_stage =
      slot.color_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
          ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
          : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  vkCmdPipelineBarrier(cmd, src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  slot.color_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
}

void OffscreenRenderTarget::cmdBarrierToShaderRead(VkCommandBuffer cmd) {
  BufferSlot& slot = activeSlot();
  if (slot.color_image == VK_NULL_HANDLE) {
    return;
  }
  if (slot.color_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    return;
  }

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = slot.color_layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = slot.color_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask =
      slot.color_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
          ? VK_ACCESS_TRANSFER_READ_BIT
          : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  const VkPipelineStageFlags src_stage =
      slot.color_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
          ? VK_PIPELINE_STAGE_TRANSFER_BIT
          : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  vkCmdPipelineBarrier(cmd, src_stage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                       0, nullptr, 0, nullptr, 1, &barrier);

  slot.color_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void OffscreenRenderTarget::cmdBarrierDepthToShaderRead(VkCommandBuffer cmd) {
  BufferSlot& slot = activeSlot();
  if (slot.depth_image == VK_NULL_HANDLE) {
    return;
  }
  if (slot.depth_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
    return;
  }

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = slot.depth_layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = slot.depth_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  slot.depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
}

}  // namespace Blunder
