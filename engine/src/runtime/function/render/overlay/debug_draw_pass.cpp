#include "runtime/function/render/overlay/debug_draw_pass.h"

#include <algorithm>
#include <vulkan/vulkan.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/vulkan/secondary_command_buffer_pool.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

DebugDrawPass::~DebugDrawPass() {
  shutdown();
}

void DebugDrawPass::initialize(VulkanContext* ctx,
                               OffscreenRenderTarget* offscreen) {
  ASSERT(ctx);
  ASSERT(offscreen);
  m_vk_context = ctx;
  m_offscreen_target = offscreen;
}

void DebugDrawPass::shutdown() {
  m_offscreen_target = nullptr;
  m_vk_context = nullptr;
}

VkRenderPass DebugDrawPass::renderPass() const {
  return m_offscreen_target != nullptr ? m_offscreen_target->getLoadRenderPass()
                                       : VK_NULL_HANDLE;
}

void DebugDrawPass::bindViewportScissor(VkCommandBuffer cmd, uint32_t width,
                                        uint32_t height) {
  VkViewport viewport{};
  viewport.width = static_cast<float>(std::max(width, 1u));
  viewport.height = static_cast<float>(std::max(height, 1u));
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  VkRect2D scissor{{0, 0}, {std::max(width, 1u), std::max(height, 1u)}};
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void DebugDrawPass::begin(VkCommandBuffer cmd, rhi::SubpassContents contents) {
  ASSERT(m_offscreen_target);
  m_offscreen_target->beginLoadRenderPass(
      cmd, SecondaryCommandBufferPool::toVkContents(contents));
}

void DebugDrawPass::end(VkCommandBuffer cmd) {
  ASSERT(m_offscreen_target);
  m_offscreen_target->endLoadRenderPass(cmd);
}

}  // namespace Blunder
