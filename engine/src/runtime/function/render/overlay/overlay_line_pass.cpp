#include "runtime/function/render/overlay/overlay_line_pass.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/overlay/overlay_line_targets.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

OverlayLinePass::~OverlayLinePass() {
  shutdown();
}

void OverlayLinePass::initialize(VulkanContext* ctx,
                                 OffscreenRenderTarget* offscreen,
                                 OverlayLineTargets* targets) {
  ASSERT(ctx);
  ASSERT(offscreen);
  ASSERT(targets);
  m_context = ctx;
  m_offscreen = offscreen;
  m_targets = targets;
  createRenderPass();
  const VkExtent2D extent = offscreen->getExtent();
  resize(extent.width, extent.height);
  m_targets->recreateFramebuffer(m_render_pass);
}

void OverlayLinePass::shutdown() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_render_pass, nullptr);
    m_render_pass = VK_NULL_HANDLE;
  }
  m_targets = nullptr;
  m_offscreen = nullptr;
  m_context = nullptr;
}

void OverlayLinePass::createRenderPass() {
  VkAttachmentDescription overlay_color{};
  overlay_color.format = VK_FORMAT_R8G8B8A8_UNORM;
  overlay_color.samples = VK_SAMPLE_COUNT_1_BIT;
  overlay_color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  overlay_color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  overlay_color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  overlay_color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription line_data{};
  line_data.format = VK_FORMAT_R16G16B16A16_UNORM;
  line_data.samples = VK_SAMPLE_COUNT_1_BIT;
  line_data.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  line_data.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  line_data.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  line_data.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkAttachmentDescription depth{};
  depth.format = VK_FORMAT_D32_SFLOAT;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference color_refs[2]{};
  color_refs[0].attachment = 0;
  color_refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_refs[1].attachment = 1;
  color_refs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_ref{};
  depth_ref.attachment = 2;
  depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 2;
  subpass.pColorAttachments = color_refs;
  subpass.pDepthStencilAttachment = &depth_ref;

  VkAttachmentDescription attachments[] = {overlay_color, line_data, depth};
  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.srcAccessMask =
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

  VkRenderPassCreateInfo rp_info{};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_info.attachmentCount = 3;
  rp_info.pAttachments = attachments;
  rp_info.subpassCount = 1;
  rp_info.pSubpasses = &subpass;
  rp_info.dependencyCount = 1;
  rp_info.pDependencies = &dep;

  const VkResult result = vkCreateRenderPass(m_context->getDevice(), &rp_info,
                                             nullptr, &m_render_pass);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[OverlayLinePass] vkCreateRenderPass failed: {}",
              static_cast<int>(result));
  }
}

void OverlayLinePass::resize(uint32_t width, uint32_t height) {
  if (m_targets) {
    m_targets->resize(width, height);
    m_targets->recreateFramebuffer(m_render_pass);
  }
}

void OverlayLinePass::begin(VkCommandBuffer cmd) {
  ASSERT(m_offscreen && m_targets && m_render_pass != VK_NULL_HANDLE);
  const VkExtent2D extent = m_targets->extent();

  VkClearValue clears[2]{};
  clears[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
  clears[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};

  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = m_render_pass;
  begin.framebuffer = m_targets->framebuffer();
  begin.renderArea.extent = extent;
  begin.clearValueCount = 2;
  begin.pClearValues = clears;
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = extent;
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void OverlayLinePass::end(VkCommandBuffer cmd) {
  vkCmdEndRenderPass(cmd);
  if (m_targets) {
    m_targets->cmdBarrierOverlayTargetsToShaderRead(cmd);
  }
}

}  // namespace Blunder
