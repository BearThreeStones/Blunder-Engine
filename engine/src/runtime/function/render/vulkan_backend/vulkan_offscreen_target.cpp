#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"

#include <vulkan/vulkan.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_gpu_buffer.h"

namespace Blunder::vulkan_backend {

VulkanOffscreenTarget::~VulkanOffscreenTarget() { shutdown(); }

void VulkanOffscreenTarget::initialize(VulkanContext* context,
                                       VulkanAllocator* allocator,
                                       const rhi::OffscreenTargetDesc& desc) {
  m_target = eastl::make_unique<OffscreenRenderTarget>();
  m_target->initialize(context, allocator, desc.width, desc.height,
                       VK_FORMAT_R8G8B8A8_UNORM);
}

void VulkanOffscreenTarget::shutdown() {
  if (m_target) {
    m_target->shutdown();
    m_target.reset();
  }
}

void VulkanOffscreenTarget::resize(uint32_t width, uint32_t height) {
  if (m_target) {
    m_target->resize(width, height);
  }
}

rhi::Extent2D VulkanOffscreenTarget::extent() const {
  rhi::Extent2D result;
  if (m_target) {
    const VkExtent2D ext = m_target->getExtent();
    result.width = ext.width;
    result.height = ext.height;
  }
  return result;
}

rhi::PixelFormat VulkanOffscreenTarget::colorFormat() const {
  return rhi::PixelFormat::R8G8B8A8_UNORM;
}

void VulkanOffscreenTarget::beginRenderPass(
    rhi::ICommandList& command_list, const rhi::ClearValue* clears,
    uint32_t clear_count) {
  ASSERT(m_target);
  auto& vk_list = static_cast<VulkanCommandList&>(command_list);
  VkCommandBuffer cmd = vk_list.vkCommandBuffer();

  VkClearValue vk_clears[2]{};
  if (clear_count > 0) {
    vk_clears[0].color = {{clears[0].color.r, clears[0].color.g, clears[0].color.b,
                           clears[0].color.a}};
  }
  if (clear_count > 1) {
    vk_clears[1].depthStencil = {clears[1].depth_stencil.depth,
                                  clears[1].depth_stencil.stencil};
  }

  VkRenderPassBeginInfo rp_info{};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_info.renderPass = m_target->getRenderPass();
  rp_info.framebuffer = m_target->getFramebuffer();
  rp_info.renderArea.offset = {0, 0};
  rp_info.renderArea.extent = m_target->getExtent();
  rp_info.clearValueCount = clear_count;
  rp_info.pClearValues = vk_clears;

  vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanOffscreenTarget::endRenderPass(rhi::ICommandList& command_list) {
  auto& vk_list = static_cast<VulkanCommandList&>(command_list);
  vkCmdEndRenderPass(vk_list.vkCommandBuffer());
}

void VulkanOffscreenTarget::transitionToCopySource(
    rhi::ICommandList& command_list) {
  auto& vk_list = static_cast<VulkanCommandList&>(command_list);
  m_target->cmdBarrierToTransferSrc(vk_list.vkCommandBuffer());
}

void VulkanOffscreenTarget::transitionToShaderRead(
    rhi::ICommandList& command_list) {
  auto& vk_list = static_cast<VulkanCommandList&>(command_list);
  m_target->cmdBarrierToShaderRead(vk_list.vkCommandBuffer());
}

void VulkanOffscreenTarget::copyColorToBuffer(rhi::ICommandList& command_list,
                                              rhi::IGpuBuffer& dst_buffer) {
  auto& vk_list = static_cast<VulkanCommandList&>(command_list);
  auto& vk_buffer = static_cast<VulkanGpuBuffer&>(dst_buffer);
  ASSERT(vk_buffer.nativeBuffer());

  const VkExtent2D ext = m_target->getExtent();
  VkBufferImageCopy copy_region{};
  copy_region.bufferOffset = 0;
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageExtent = {ext.width, ext.height, 1};

  vkCmdCopyImageToBuffer(vk_list.vkCommandBuffer(), m_target->getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         vk_buffer.nativeBuffer()->getBuffer(), 1,
                         &copy_region);
}

void VulkanOffscreenTarget::markPostRenderPassShaderRead() {
  if (m_target) {
    m_target->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }
}

}  // namespace Blunder::vulkan_backend
