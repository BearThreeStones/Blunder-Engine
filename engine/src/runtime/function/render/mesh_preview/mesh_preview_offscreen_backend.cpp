#include "runtime/function/render/mesh_preview/mesh_preview_offscreen_backend.h"

#include <cstring>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/i_render_backend.h"
#include "runtime/function/render/rhi/i_render_device.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"
#include "runtime/function/render/vulkan_backend/vulkan_render_backend.h"

namespace Blunder {

MeshPreviewOffscreenBackend::MeshPreviewOffscreenBackend() = default;

MeshPreviewOffscreenBackend::~MeshPreviewOffscreenBackend() { shutdown(); }

bool MeshPreviewOffscreenBackend::initialize(
    rhi::IRenderBackend* render_backend) {
  shutdown();
  if (render_backend == nullptr ||
      render_backend->type() != rhi::RenderBackendType::Vulkan) {
    return false;
  }
  m_render_backend = render_backend;
  return true;
}

void MeshPreviewOffscreenBackend::shutdown() {
  if (m_readback_staging) {
    m_readback_staging->destroy();
    m_readback_staging.reset();
  }
  if (m_offscreen) {
    static_cast<vulkan_backend::VulkanOffscreenTarget*>(m_offscreen.get())
        ->shutdown();
    m_offscreen.reset();
  }
  m_render_backend = nullptr;
  m_width = 0;
  m_height = 0;
}

bool MeshPreviewOffscreenBackend::ensureResources(uint32_t width,
                                                  uint32_t height) {
  if (m_render_backend == nullptr || width == 0 || height == 0) {
    return false;
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanAllocator* allocator = backend->nativeAllocator();
  if (allocator == nullptr) {
    return false;
  }

  if (!m_offscreen) {
    rhi::OffscreenTargetDesc desc{};
    desc.width = width;
    desc.height = height;
    m_offscreen = backend->device().createOffscreenTarget(desc);
  } else if (m_width != width || m_height != height) {
    m_offscreen->resize(width, height);
  }

  if (!m_readback_staging || m_width != width || m_height != height) {
    if (m_readback_staging) {
      m_readback_staging->destroy();
    } else {
      m_readback_staging = eastl::make_unique<VulkanBuffer>();
    }
    const VkDeviceSize bytes =
        static_cast<VkDeviceSize>(width) * height * 4u;
    m_readback_staging->create(allocator, bytes,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VMA_MEMORY_USAGE_GPU_TO_CPU);
  }

  m_width = width;
  m_height = height;
  return m_offscreen != nullptr && m_readback_staging != nullptr;
}

bool MeshPreviewOffscreenBackend::renderMeshPreview(
    const MeshAsset& mesh, const MeshPreviewRenderRequest& request,
    const MeshPreviewCameraFrame& framing,
    const MeshPreviewStudioLights& lights, MeshPreviewPoseMode pose_mode,
    eastl::vector<uint8_t>& out_rgba) {
  (void)mesh;
  (void)framing;
  (void)lights;
  (void)pose_mode;

  out_rgba.clear();
  if (!ensureResources(request.width, request.height)) {
    return false;
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanContext* context = backend->nativeVulkanContext();
  VulkanAllocator* allocator = backend->nativeAllocator();
  if (context == nullptr || allocator == nullptr) {
    return false;
  }

  VkCommandBuffer command_buffer = context->beginImmediateCommands();
  vulkan_backend::VulkanCommandList command_list;
  command_list.bind(context, command_buffer);

  const rhi::ClearValue clears[2] = {
      {.color = {0.055f, 0.065f, 0.085f, 1.0f}},
      {.depth_stencil = {1.0f, 0u}},
  };
  m_offscreen->beginRenderPass(command_list, clears, 2u);
  m_offscreen->endRenderPass(command_list);
  m_offscreen->markPostRenderPassShaderRead();
  m_offscreen->transitionToCopySource(command_list);

  auto* vk_target = static_cast<vulkan_backend::VulkanOffscreenTarget*>(
      m_offscreen.get());
  OffscreenRenderTarget* native_target = vk_target->nativeTarget();
  if (native_target == nullptr) {
    context->endImmediateCommands(command_buffer);
    return false;
  }

  VkBufferImageCopy copy_region{};
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageExtent = {request.width, request.height, 1};
  vkCmdCopyImageToBuffer(command_buffer, native_target->getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         m_readback_staging->getBuffer(), 1, &copy_region);
  m_offscreen->transitionToShaderRead(command_list);
  context->endImmediateCommands(command_buffer);

  const VkDeviceSize byte_count =
      static_cast<VkDeviceSize>(request.width) * request.height * 4u;
  vmaInvalidateAllocation(allocator->getAllocator(),
                          m_readback_staging->getAllocation(), 0, byte_count);
  void* mapped = nullptr;
  if (vmaMapMemory(allocator->getAllocator(),
                   m_readback_staging->getAllocation(),
                   &mapped) != VK_SUCCESS ||
      mapped == nullptr) {
    return false;
  }

  out_rgba.resize(static_cast<size_t>(byte_count));
  std::memcpy(out_rgba.data(), mapped, static_cast<size_t>(byte_count));
  vmaUnmapMemory(allocator->getAllocator(),
                 m_readback_staging->getAllocation());
  return true;
}

}  // namespace Blunder
