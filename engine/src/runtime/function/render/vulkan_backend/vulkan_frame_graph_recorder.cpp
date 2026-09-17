#include "runtime/function/render/vulkan_backend/vulkan_frame_graph_recorder.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_image.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_gpu_buffer.h"
#include "runtime/function/render/vulkan_backend/vulkan_gpu_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_imported_gpu_texture.h"

namespace Blunder::vulkan_backend {
namespace {

void mapState(const FrameGraphResourceState& state, VkImageLayout& layout,
              VkAccessFlags& access, VkPipelineStageFlags& stage) {
  if (state.undefined) {
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
    access = 0;
    stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    return;
  }

  const bool write = state.access == FrameGraphAccessKind::Write;
  switch (state.usage) {
    case FrameGraphUsage::Sampled:
      layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      access = write ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
      if (write) {
        layout = VK_IMAGE_LAYOUT_GENERAL;
      }
      stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      return;
    case FrameGraphUsage::ColorAttachment:
      layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      access = write ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                     : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
      stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      return;
    case FrameGraphUsage::DepthAttachment:
      layout = write ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                     : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      access = write ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                     : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      if (!write) {
        stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
      return;
    case FrameGraphUsage::Storage:
      layout = VK_IMAGE_LAYOUT_GENERAL;
      access = write ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
      stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      return;
  }

  LOG_FATAL("[VulkanFrameGraphRecorder] unmapped Frame graph usage {}",
            static_cast<int>(state.usage));
  layout = VK_IMAGE_LAYOUT_UNDEFINED;
  access = 0;
  stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}

}  // namespace

FrameGraphVulkanMapping mapFrameGraphBarrier(
    const FrameGraphBarrier& barrier, FrameGraphResourceShape shape) {
  FrameGraphVulkanMapping mapped{};
  mapped.is_buffer = shape == FrameGraphResourceShape::Buffer;
  mapState(barrier.from, mapped.old_layout, mapped.src_access, mapped.src_stage);
  mapState(barrier.to, mapped.new_layout, mapped.dst_access, mapped.dst_stage);
  if (mapped.is_buffer) {
    mapped.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    mapped.new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  }
  return mapped;
}

void VulkanFrameGraphRecorder::bind(FrameGraph* graph,
                                    VkCommandBuffer command_buffer) {
  m_graph = graph;
  m_command_buffer = command_buffer;
}

void VulkanFrameGraphRecorder::pipelineBarrier(
    const FrameGraphBarrier& barrier) {
  if (m_graph == nullptr || m_command_buffer == VK_NULL_HANDLE) {
    LOG_FATAL("[VulkanFrameGraphRecorder] pipelineBarrier without bind");
  }

  const FrameGraphResourceShape shape = m_graph->resourceShape(barrier.resource);
  const FrameGraphVulkanMapping mapped = mapFrameGraphBarrier(barrier, shape);

  if (mapped.is_buffer) {
    rhi::IGpuBuffer* buffer = m_graph->resolvedBuffer(barrier.resource);
    auto* gpu = static_cast<VulkanGpuBuffer*>(buffer);
    VkBuffer vk_buffer =
        (gpu != nullptr && gpu->nativeBuffer() != nullptr)
            ? gpu->nativeBuffer()->getBuffer()
            : VK_NULL_HANDLE;
    if (vk_buffer == VK_NULL_HANDLE) {
      LOG_FATAL(
          "[VulkanFrameGraphRecorder] buffer barrier missing VkBuffer "
          "(handle {})",
          barrier.resource.index);
    }
    VkBufferMemoryBarrier buffer_barrier{};
    buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    buffer_barrier.srcAccessMask = mapped.src_access;
    buffer_barrier.dstAccessMask = mapped.dst_access;
    buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    buffer_barrier.buffer = vk_buffer;
    buffer_barrier.offset = 0;
    buffer_barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(m_command_buffer, mapped.src_stage, mapped.dst_stage,
                         0, 0, nullptr, 1, &buffer_barrier, 0, nullptr);
    return;
  }

  rhi::IGpuTexture* texture = m_graph->resolvedTexture(barrier.resource);
  VkImage image = VK_NULL_HANDLE;
  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  if (m_graph->resourceKind(barrier.resource) ==
      FrameGraphResourceKind::External) {
    auto* imported = static_cast<VulkanImportedGpuTexture*>(texture);
    if (imported != nullptr) {
      image = imported->vkImage();
      aspect = imported->aspectMask();
    }
  } else if (texture != nullptr) {
    auto* gpu = static_cast<VulkanGpuTexture*>(texture);
    if (gpu != nullptr && gpu->nativeTexture() != nullptr) {
      const VulkanImage& native = gpu->nativeTexture()->getImage();
      image = native.getImage();
      aspect = native.getAspectMask();
    }
  }
  if (image == VK_NULL_HANDLE) {
    LOG_FATAL(
        "[VulkanFrameGraphRecorder] texture barrier missing VkImage "
        "(handle {})",
        barrier.resource.index);
  }

  VkImageMemoryBarrier image_barrier{};
  image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_barrier.srcAccessMask = mapped.src_access;
  image_barrier.dstAccessMask = mapped.dst_access;
  image_barrier.oldLayout = mapped.old_layout;
  image_barrier.newLayout = mapped.new_layout;
  image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_barrier.image = image;
  image_barrier.subresourceRange.aspectMask = aspect;
  image_barrier.subresourceRange.baseMipLevel = 0;
  image_barrier.subresourceRange.levelCount = 1;
  image_barrier.subresourceRange.baseArrayLayer = 0;
  image_barrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(m_command_buffer, mapped.src_stage, mapped.dst_stage, 0,
                       0, nullptr, 0, nullptr, 1, &image_barrier);
}

}  // namespace Blunder::vulkan_backend
