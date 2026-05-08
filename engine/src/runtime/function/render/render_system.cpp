#include "runtime/function/render/render_system.h"

#include <slang.h>

#include <cstdint>
#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "EASTL/memory.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/event/event.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

namespace {

const uint64_t k_fence_wait_timeout_ns = 1000000000ULL;

struct GlobalUniformData {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};

void copyBuffer(VkDevice device, VkCommandPool command_pool, VkQueue queue,
                VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandPool = command_pool;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  const VkResult alloc_result =
      vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);
  if (alloc_result != VK_SUCCESS) {
    LOG_FATAL("[RenderSystem::copyBuffer] vkAllocateCommandBuffers failed: {}",
              static_cast<int>(alloc_result));
  }

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(command_buffer, &begin_info);

  VkBufferCopy copy_region{};
  copy_region.size = size;
  vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);
  vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

const Vertex k_quad_vertices[] = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                  {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                  {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                  {{-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}}};

const uint16_t k_quad_indices[] = {0, 1, 2, 2, 3, 0};

constexpr uint32_t k_default_viewport_w = 1024;
constexpr uint32_t k_default_viewport_h = 720;

}  // namespace

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() { shutdown(); }

void RenderSystem::initialize(const RenderSystemInitInfo& info) {
  ASSERT(info.window_system);

  m_window_system = info.window_system;

  m_slang_compiler = eastl::make_shared<SlangCompiler>();
  m_slang_compiler->initialize();

  m_context = eastl::make_shared<VulkanContext>();
  VulkanContextCreateInfo context_info;
  context_info.window_system = info.window_system;
  context_info.enable_validation = info.enable_validation;
  m_context->initialize(context_info);

  m_allocator = eastl::make_shared<VulkanAllocator>();
  m_allocator->initialize(m_context.get());

  m_sync = eastl::make_shared<VulkanSync>();
  // No swapchain images -> we only use the per-frame in-flight fences.
  m_sync->initialize(m_context.get(), VulkanSync::k_max_frames_in_flight);

  m_offscreen_rt = eastl::make_unique<OffscreenRenderTarget>();
  m_offscreen_rt->initialize(m_context.get(), m_allocator.get(),
                             k_default_viewport_w, k_default_viewport_h);

  m_pipeline = eastl::make_shared<VulkanPipeline>();
  m_pipeline->initialize(m_context.get(), m_slang_compiler.get(),
                         m_offscreen_rt->getRenderPass());

  m_editor_camera = eastl::make_unique<EditorCamera>(m_window_system);

  // Vertex/index buffers (rotating quad demo geometry).
  eastl::unique_ptr<VulkanBuffer> staging_buffer =
      eastl::make_unique<VulkanBuffer>();
  staging_buffer->create(m_allocator.get(), sizeof(k_quad_vertices),
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
  staging_buffer->upload(k_quad_vertices, sizeof(k_quad_vertices));

  m_vertex_buffer = eastl::make_unique<VulkanBuffer>();
  m_vertex_buffer->create(
      m_allocator.get(), sizeof(k_quad_vertices),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  copyBuffer(m_context->getDevice(), m_pipeline->getCommandPool(),
             m_context->getGraphicsQueue(), staging_buffer->getBuffer(),
             m_vertex_buffer->getBuffer(), sizeof(k_quad_vertices));

  staging_buffer->destroy();

  eastl::unique_ptr<VulkanBuffer> index_staging_buffer =
      eastl::make_unique<VulkanBuffer>();
  index_staging_buffer->create(m_allocator.get(), sizeof(k_quad_indices),
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VMA_MEMORY_USAGE_CPU_TO_GPU);
  index_staging_buffer->upload(k_quad_indices, sizeof(k_quad_indices));

  m_index_buffer = eastl::make_unique<VulkanBuffer>();
  m_index_buffer->create(
      m_allocator.get(), sizeof(k_quad_indices),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  copyBuffer(m_context->getDevice(), m_pipeline->getCommandPool(),
             m_context->getGraphicsQueue(), index_staging_buffer->getBuffer(),
             m_index_buffer->getBuffer(), sizeof(k_quad_indices));

  index_staging_buffer->destroy();

  m_uniform_buffers.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_allocator.get(), sizeof(GlobalUniformData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = VulkanSync::k_max_frames_in_flight;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  pool_info.maxSets = VulkanSync::k_max_frames_in_flight;

  const VkResult descriptor_pool_result = vkCreateDescriptorPool(
      m_context->getDevice(), &pool_info, nullptr, &m_descriptor_pool);
  if (descriptor_pool_result != VK_SUCCESS) {
    LOG_FATAL("[RenderSystem::initialize] vkCreateDescriptorPool failed: {}",
              static_cast<int>(descriptor_pool_result));
  }

  eastl::vector<VkDescriptorSetLayout> layouts(
      VulkanSync::k_max_frames_in_flight, m_pipeline->getDescriptorSetLayout());
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = m_descriptor_pool;
  alloc_info.descriptorSetCount = VulkanSync::k_max_frames_in_flight;
  alloc_info.pSetLayouts = layouts.data();

  m_descriptor_sets.resize(VulkanSync::k_max_frames_in_flight);
  const VkResult descriptor_set_result = vkAllocateDescriptorSets(
      m_context->getDevice(), &alloc_info, m_descriptor_sets.data());
  if (descriptor_set_result != VK_SUCCESS) {
    LOG_FATAL("[RenderSystem::initialize] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(descriptor_set_result));
  }

  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_uniform_buffers[i]->getBuffer();
    buffer_info.offset = 0;
    buffer_info.range = sizeof(GlobalUniformData);

    VkWriteDescriptorSet descriptor_write{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = m_descriptor_sets[i];
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_write.descriptorCount = 1;
    descriptor_write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(m_context->getDevice(), 1, &descriptor_write, 0,
                           nullptr);
  }

  recreateReadbackStaging(k_default_viewport_w, k_default_viewport_h);
}

void RenderSystem::recreateReadbackStaging(uint32_t width, uint32_t height) {
  ASSERT(m_allocator);
  if (width == 0 || height == 0) {
    return;
  }

  for (eastl::unique_ptr<VulkanBuffer>& buf : m_readback_staging) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_readback_staging.clear();

  const VkDeviceSize bytes =
      static_cast<VkDeviceSize>(width) * height * 4u;
  m_readback_staging.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_readback_staging[i] = eastl::make_unique<VulkanBuffer>();
    m_readback_staging[i]->create(m_allocator.get(), bytes,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VMA_MEMORY_USAGE_GPU_TO_CPU);
  }
  m_readback_width = width;
  m_readback_height = height;
  m_readback_pixels.resize(static_cast<size_t>(bytes));
}

void RenderSystem::resizeOffscreenIfNeeded(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return;
  }
  const VkExtent2D current = m_offscreen_rt->getExtent();
  if (current.width == width && current.height == height) {
    return;
  }

  vkDeviceWaitIdle(m_context->getDevice());
  m_offscreen_rt->resize(width, height);
  recreateReadbackStaging(width, height);
}

void RenderSystem::shutdown() {
  if (!m_context) {
    return;
  }

  vkDeviceWaitIdle(m_context->getDevice());

  for (eastl::unique_ptr<VulkanBuffer>& buf : m_readback_staging) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_readback_staging.clear();
  m_readback_pixels.clear();

  if (m_offscreen_rt) {
    m_offscreen_rt->shutdown();
    m_offscreen_rt.reset();
  }

  if (m_vertex_buffer) {
    m_vertex_buffer->destroy();
    m_vertex_buffer.reset();
  }

  if (m_index_buffer) {
    m_index_buffer->destroy();
    m_index_buffer.reset();
  }

  for (eastl::unique_ptr<VulkanBuffer>& uniform_buffer : m_uniform_buffers) {
    if (uniform_buffer) {
      uniform_buffer->destroy();
      uniform_buffer.reset();
    }
  }
  m_uniform_buffers.clear();

  m_descriptor_sets.clear();

  if (m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_context->getDevice(), m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
  }

  if (m_pipeline) {
    m_pipeline->shutdown();
    m_pipeline.reset();
  }

  m_editor_camera.reset();

  if (m_sync) {
    m_sync->shutdown();
    m_sync.reset();
  }

  if (m_allocator) {
    m_allocator->shutdown();
    m_allocator.reset();
  }

  m_context->shutdown();
  m_context.reset();

  if (m_slang_compiler) {
    m_slang_compiler->shutdown();
    m_slang_compiler.reset();
  }

  m_window_system = nullptr;
  m_current_frame = 0;
}

void RenderSystem::tick(float delta_time, uint32_t target_width,
                        uint32_t target_height, SlintSystem* slint_system) {
  m_elapsed_time += delta_time;

  resizeOffscreenIfNeeded(target_width, target_height);

  const VkExtent2D offscreen_extent = m_offscreen_rt->getExtent();
  if (offscreen_extent.width == 0 || offscreen_extent.height == 0) {
    return;
  }

  GlobalUniformData ubo{};
  ubo.model = glm::rotate(glm::mat4(1.0f), m_elapsed_time * glm::radians(90.0f),
                          glm::vec3(0.0f, 0.0f, 1.0f));
  if (m_editor_camera) {
    m_editor_camera->setViewportSize(static_cast<float>(offscreen_extent.width),
                                     static_cast<float>(offscreen_extent.height));
    m_editor_camera->onUpdate(delta_time);
    ubo.view = m_editor_camera->getViewMatrix();
    ubo.projection = m_editor_camera->getProjectionMatrix();
  } else {
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                           glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
    const float aspect = static_cast<float>(offscreen_extent.width) /
                         static_cast<float>(offscreen_extent.height);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
    proj[1][1] *= -1.0f;
    ubo.projection = proj;
  }

  m_uniform_buffers[m_current_frame]->upload(&ubo, sizeof(ubo));

  VkDevice device = m_context->getDevice();
  VkFence in_flight_fence = m_sync->getInFlightFence(m_current_frame);
  vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE,
                  k_fence_wait_timeout_ns);
  vkResetFences(device, 1, &in_flight_fence);

  VkCommandBuffer command_buffer =
      m_pipeline->getCommandBuffer(m_current_frame);
  vkResetCommandBuffer(command_buffer, 0);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(command_buffer, &begin_info);

  // ===== Pass 1: 3D scene -> offscreen render target =====
  {
    VkClearValue offscreen_clear{};
    offscreen_clear.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

    VkRenderPassBeginInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = m_offscreen_rt->getRenderPass();
    rp_info.framebuffer = m_offscreen_rt->getFramebuffer();
    rp_info.renderArea.offset = {0, 0};
    rp_info.renderArea.extent = offscreen_extent;
    rp_info.clearValueCount = 1;
    rp_info.pClearValues = &offscreen_clear;

    vkCmdBeginRenderPass(command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_pipeline->getGraphicsPipeline());
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline->getPipelineLayout(), 0, 1,
                            &m_descriptor_sets[m_current_frame], 0, nullptr);

    VkViewport viewport{};
    viewport.width = static_cast<float>(offscreen_extent.width);
    viewport.height = static_cast<float>(offscreen_extent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = offscreen_extent;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkBuffer vertex_buffers[] = {m_vertex_buffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
    vkCmdBindIndexBuffer(command_buffer, m_index_buffer->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(command_buffer,
                     static_cast<uint32_t>(sizeof(k_quad_indices) /
                                           sizeof(k_quad_indices[0])),
                     1, 0, 0, 0);
    vkCmdEndRenderPass(command_buffer);
    m_offscreen_rt->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  // ===== Readback: SHADER_READ_ONLY -> TRANSFER_SRC -> copy -> SHADER_READ =====
  m_offscreen_rt->cmdBarrierToTransferSrc(command_buffer);

  VkBufferImageCopy copy_region{};
  copy_region.bufferOffset = 0;
  copy_region.bufferRowLength = 0;
  copy_region.bufferImageHeight = 0;
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageOffset = {0, 0, 0};
  copy_region.imageExtent.width = offscreen_extent.width;
  copy_region.imageExtent.height = offscreen_extent.height;
  copy_region.imageExtent.depth = 1;

  vkCmdCopyImageToBuffer(
      command_buffer, m_offscreen_rt->getImage(),
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      m_readback_staging[m_current_frame]->getBuffer(), 1, &copy_region);

  m_offscreen_rt->cmdBarrierToShaderRead(command_buffer);

  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submit_info,
                in_flight_fence);

  // Synchronously wait so we can map the staging buffer right away. This is
  // simple but stalls the pipeline; cleanup phase can pingpong over the
  // k_max_frames_in_flight buffers if profiling shows it matters.
  vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE,
                  k_fence_wait_timeout_ns);

  // Map the staging buffer and forward pixels to Slint.
  if (slint_system) {
    void* mapped = nullptr;
    const VkResult mr = vmaMapMemory(
        m_allocator->getAllocator(),
        m_readback_staging[m_current_frame]->getAllocation(), &mapped);
    if (mr == VK_SUCCESS && mapped) {
      const size_t bytes = static_cast<size_t>(offscreen_extent.width) *
                           offscreen_extent.height * 4u;
      if (m_readback_pixels.size() < bytes) {
        m_readback_pixels.resize(bytes);
      }
      std::memcpy(m_readback_pixels.data(), mapped, bytes);
      vmaUnmapMemory(m_allocator->getAllocator(),
                     m_readback_staging[m_current_frame]->getAllocation());

      slint_system->setViewportImage(m_readback_pixels.data(),
                                     offscreen_extent.width,
                                     offscreen_extent.height);
    }
  }

  m_current_frame = (m_current_frame + 1) % VulkanSync::k_max_frames_in_flight;
}

void RenderSystem::onEvent(Event& event) {
  if (m_editor_camera) {
    m_editor_camera->onEvent(event);
  }
}

}  // namespace Blunder
