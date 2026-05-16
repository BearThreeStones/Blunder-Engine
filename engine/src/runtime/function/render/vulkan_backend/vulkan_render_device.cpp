#include "runtime/function/render/vulkan_backend/vulkan_render_device.h"

#include <vulkan/vulkan.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/rhi/i_command_list.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_gpu_buffer.h"
#include "runtime/function/render/vulkan_backend/vulkan_gpu_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder::vulkan_backend {

void VulkanRenderDevice::bind(VulkanContext* context,
                              VulkanAllocator* allocator) {
  m_context = context;
  m_allocator = allocator;
}

void VulkanRenderDevice::initialize(const rhi::RenderDeviceDesc& desc) {
  (void)desc;
}

void VulkanRenderDevice::shutdown() {}

eastl::unique_ptr<rhi::IOffscreenRenderTarget>
VulkanRenderDevice::createOffscreenTarget(const rhi::OffscreenTargetDesc& desc) {
  auto target = eastl::make_unique<VulkanOffscreenTarget>();
  target->initialize(m_context, m_allocator, desc);
  return target;
}

eastl::unique_ptr<rhi::IGpuBuffer> VulkanRenderDevice::createBuffer(
    const rhi::BufferDesc& desc) {
  auto buffer = eastl::make_unique<VulkanBuffer>();
  VkBufferUsageFlags usage = 0;
  if (desc.uniform_buffer) {
    usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if (desc.copy_source) {
    usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }

  VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
  if (desc.host_visible) {
    memory_usage = desc.copy_source ? VMA_MEMORY_USAGE_GPU_TO_CPU
                                    : VMA_MEMORY_USAGE_CPU_TO_GPU;
  }

  buffer->create(m_allocator, static_cast<VkDeviceSize>(desc.size), usage,
                 memory_usage);

  auto wrapper = eastl::make_unique<VulkanGpuBuffer>();
  wrapper->setBuffer(eastl::move(buffer));
  return wrapper;
}

eastl::unique_ptr<rhi::IGpuTexture> VulkanRenderDevice::createTextureFromAsset(
    const Texture2DAsset* asset) {
  if (!asset) {
    return nullptr;
  }
  auto texture = eastl::make_unique<VulkanTexture>();
  texture->createFromTexture2DAsset(m_context, m_allocator, *asset);
  auto wrapper = eastl::make_unique<VulkanGpuTexture>();
  wrapper->setTexture(eastl::move(texture));
  return wrapper;
}

eastl::unique_ptr<rhi::ICommandList>
VulkanRenderDevice::beginImmediateCommandList() {
  LOG_FATAL(
      "[VulkanRenderDevice] beginImmediateCommandList is not used; call "
      "VulkanContext::beginImmediateCommands via VulkanRenderBackend");
  return nullptr;
}

}  // namespace Blunder::vulkan_backend
