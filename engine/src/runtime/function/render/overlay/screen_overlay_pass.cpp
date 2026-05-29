#include "runtime/function/render/overlay/screen_overlay_pass.h"

#include <vulkan/vulkan.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

namespace {

VkRenderPass createScreenOverlayRenderPass(VkDevice device,
                                           VkFormat color_format) {
  VkAttachmentDescription color_attachment{};
  color_attachment.format = color_format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depth_attachment{};
  depth_attachment.format = VK_FORMAT_D32_SFLOAT;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  depth_attachment.finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

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

  VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};
  VkRenderPassCreateInfo rp_info{};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_info.attachmentCount = 2;
  rp_info.pAttachments = attachments;
  rp_info.subpassCount = 1;
  rp_info.pSubpasses = &subpass;
  rp_info.dependencyCount = 2;
  rp_info.pDependencies = dependencies;

  VkRenderPass render_pass = VK_NULL_HANDLE;
  const VkResult result =
      vkCreateRenderPass(device, &rp_info, nullptr, &render_pass);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[ScreenOverlayPass] vkCreateRenderPass failed: {}",
              static_cast<int>(result));
  }
  return render_pass;
}

}  // namespace

ScreenOverlayPass::~ScreenOverlayPass() {
  shutdown();
}

void ScreenOverlayPass::initialize(VulkanContext* ctx,
                                   OffscreenRenderTarget* offscreen) {
  ASSERT(ctx);
  ASSERT(offscreen);
  m_vk_context = ctx;
  m_offscreen_target = offscreen;
  m_render_pass =
      createScreenOverlayRenderPass(ctx->getDevice(), offscreen->getFormat());
}

void ScreenOverlayPass::shutdown() {
  if (m_vk_context == nullptr) {
    return;
  }
  if (m_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_vk_context->getDevice(), m_render_pass, nullptr);
    m_render_pass = VK_NULL_HANDLE;
  }
  m_offscreen_target = nullptr;
  m_vk_context = nullptr;
}

void ScreenOverlayPass::begin(VkCommandBuffer cmd) {
  ASSERT(m_offscreen_target);
  ASSERT(m_render_pass != VK_NULL_HANDLE);

  const VkExtent2D extent = m_offscreen_target->getExtent();
  VkRenderPassBeginInfo rp_begin{};
  rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin.renderPass = m_render_pass;
  rp_begin.framebuffer = m_offscreen_target->getFramebuffer();
  rp_begin.renderArea.offset = {0, 0};
  rp_begin.renderArea.extent = extent;
  rp_begin.clearValueCount = 0;
  vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
}

void ScreenOverlayPass::end(VkCommandBuffer cmd) {
  vkCmdEndRenderPass(cmd);
  if (m_offscreen_target != nullptr) {
    m_offscreen_target->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_offscreen_target->setDepthLayout(
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
  }
}

}  // namespace Blunder
