#include "runtime/function/render/overlay/outline_targets.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

namespace {

void createImage2D(VulkanContext* context, VulkanAllocator* allocator,
                   uint32_t width, uint32_t height, VkFormat format,
                   VkImageUsageFlags usage, VkImageAspectFlags aspect,
                   VkImage* out_image, VmaAllocation* out_allocation,
                   VkImageView* out_view) {
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
  vmaCreateImage(allocator->getAllocator(), &image_info, &alloc_info, out_image,
                 out_allocation, nullptr);

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = *out_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = format;
  view_info.subresourceRange.aspectMask = aspect;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;
  vkCreateImageView(context->getDevice(), &view_info, nullptr, out_view);
}

}  // namespace

OutlineTargets::~OutlineTargets() {
  shutdown();
}

VkFormat OutlineTargets::pickObjectIdFormat(VkPhysicalDevice physical_device) {
  const VkFormat candidates[] = {VK_FORMAT_R16_UINT, VK_FORMAT_R32_UINT};
  for (VkFormat format : candidates) {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);
    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
      return format;
    }
  }
  return VK_FORMAT_R32_UINT;
}

void OutlineTargets::initialize(VulkanContext* ctx, VulkanAllocator* alloc) {
  ASSERT(ctx);
  ASSERT(alloc);
  m_context = ctx;
  m_allocator = alloc;
  m_object_id_format =
      pickObjectIdFormat(ctx->getPhysicalDevice());
  createPrepassRenderPass();
}

void OutlineTargets::shutdown() {
  destroyResources();
  destroyPrepassRenderPass();
  m_allocator = nullptr;
  m_context = nullptr;
}

void OutlineTargets::resize(uint32_t width, uint32_t height) {
  if (m_width == width && m_height == height &&
      m_prepass_framebuffer != VK_NULL_HANDLE) {
    return;
  }
  destroyResources();
  if (width == 0 || height == 0 || m_context == nullptr) {
    return;
  }

  m_width = width;
  m_height = height;

  const VkImageUsageFlags id_usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  createImage2D(m_context, m_allocator, width, height, m_object_id_format,
                id_usage, VK_IMAGE_ASPECT_COLOR_BIT, &m_object_id_image,
                &m_object_id_allocation, &m_object_id_view);
  m_object_id_layout = VK_IMAGE_LAYOUT_UNDEFINED;

  const VkImageUsageFlags depth_usage =
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  createImage2D(m_context, m_allocator, width, height, VK_FORMAT_D32_SFLOAT,
                depth_usage, VK_IMAGE_ASPECT_DEPTH_BIT, &m_outline_depth_image,
                &m_outline_depth_allocation, &m_outline_depth_view);
  m_outline_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;

  recreatePrepassFramebuffer();
}

void OutlineTargets::createPrepassRenderPass() {
  if (m_context == nullptr) {
    return;
  }

  VkAttachmentDescription object_id{};
  object_id.format = m_object_id_format;
  object_id.samples = VK_SAMPLE_COUNT_1_BIT;
  object_id.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  object_id.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  object_id.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  object_id.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depth{};
  depth.format = VK_FORMAT_D32_SFLOAT;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_ref{};
  depth_ref.attachment = 1;
  depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;
  subpass.pDepthStencilAttachment = &depth_ref;

  VkAttachmentDescription attachments[] = {object_id, depth};
  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo rp_info{};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_info.attachmentCount = 2;
  rp_info.pAttachments = attachments;
  rp_info.subpassCount = 1;
  rp_info.pSubpasses = &subpass;
  rp_info.dependencyCount = 1;
  rp_info.pDependencies = &dep;

  const VkResult result = vkCreateRenderPass(m_context->getDevice(), &rp_info,
                                             nullptr, &m_prepass_render_pass);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[OutlineTargets] vkCreateRenderPass failed: {}",
              static_cast<int>(result));
  }
}

void OutlineTargets::destroyPrepassRenderPass() {
  if (m_context != nullptr && m_prepass_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_context->getDevice(), m_prepass_render_pass, nullptr);
    m_prepass_render_pass = VK_NULL_HANDLE;
  }
}

void OutlineTargets::recreatePrepassFramebuffer() {
  if (m_context == nullptr || m_prepass_render_pass == VK_NULL_HANDLE ||
      m_width == 0 || m_height == 0) {
    return;
  }

  VkDevice device = m_context->getDevice();
  if (m_prepass_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, m_prepass_framebuffer, nullptr);
    m_prepass_framebuffer = VK_NULL_HANDLE;
  }

  VkImageView attachments[] = {m_object_id_view, m_outline_depth_view};
  VkFramebufferCreateInfo fb_info{};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.renderPass = m_prepass_render_pass;
  fb_info.attachmentCount = 2;
  fb_info.pAttachments = attachments;
  fb_info.width = m_width;
  fb_info.height = m_height;
  fb_info.layers = 1;
  const VkResult result =
      vkCreateFramebuffer(device, &fb_info, nullptr, &m_prepass_framebuffer);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[OutlineTargets] vkCreateFramebuffer failed: {}",
              static_cast<int>(result));
  }
}

void OutlineTargets::destroyResources() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();

  if (m_prepass_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, m_prepass_framebuffer, nullptr);
    m_prepass_framebuffer = VK_NULL_HANDLE;
  }

  auto destroy_view_image = [&](VkImageView& view, VkImage& image,
                                VmaAllocation& alloc, VkImageLayout& layout) {
  if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, view, nullptr);
      view = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE) {
      vmaDestroyImage(m_allocator->getAllocator(), image, alloc);
      image = VK_NULL_HANDLE;
      alloc = VK_NULL_HANDLE;
    }
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  destroy_view_image(m_object_id_view, m_object_id_image, m_object_id_allocation,
                     m_object_id_layout);
  destroy_view_image(m_outline_depth_view, m_outline_depth_image,
                     m_outline_depth_allocation, m_outline_depth_layout);

  m_width = 0;
  m_height = 0;
}

VkExtent2D OutlineTargets::extent() const {
  return {m_width, m_height};
}

void OutlineTargets::cmdBarrierToShaderRead(VkCommandBuffer cmd) {
  auto barrier = [&](VkImage image, VkImageLayout& tracked,
                     VkImageAspectFlags aspect) {
    if (image == VK_NULL_HANDLE ||
        tracked == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      return;
    }
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = tracked;
    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b.srcAccessMask = aspect == VK_IMAGE_ASPECT_COLOR_BIT
                          ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                          : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    b.image = image;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    const VkPipelineStageFlags src_stage =
        aspect == VK_IMAGE_ASPECT_COLOR_BIT
            ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
            : VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    vkCmdPipelineBarrier(cmd, src_stage, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &b);
    tracked = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  };

  barrier(m_object_id_image, m_object_id_layout, VK_IMAGE_ASPECT_COLOR_BIT);
  barrier(m_outline_depth_image, m_outline_depth_layout,
          VK_IMAGE_ASPECT_DEPTH_BIT);
}

}  // namespace Blunder
