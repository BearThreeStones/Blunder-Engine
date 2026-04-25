#include "runtime/function/render/render_system.h"

#include <cstdint>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "EASTL/memory.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_swapchain.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Blunder {

namespace {

// fence 等待超时时间（1s）
const uint64_t k_fence_wait_timeout_ns = 1000000000ULL;

// UBO (统一缓冲对象)
struct GlobalUniformData {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

// 一次性提交的 Buffer 拷贝工具函数：用于将 staging buffer 数据拷贝到 GPU 本地
// buffer
void copyBuffer(VkDevice device, VkCommandPool command_pool, VkQueue queue,
                VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
  // 分配临时命令缓冲
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

  // 开始记录一次性命令
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  const VkResult begin_result =
      vkBeginCommandBuffer(command_buffer, &begin_info);
  if (begin_result != VK_SUCCESS) {
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    LOG_FATAL("[RenderSystem::copyBuffer] vkBeginCommandBuffer failed: {}",
              static_cast<int>(begin_result));
  }

  // 记录 buffer copy 命令
  VkBufferCopy copy_region{};
  copy_region.size = size;
  vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

  // 结束命令记录
  const VkResult end_result = vkEndCommandBuffer(command_buffer);
  if (end_result != VK_SUCCESS) {
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    LOG_FATAL("[RenderSystem::copyBuffer] vkEndCommandBuffer failed: {}",
              static_cast<int>(end_result));
  }

  // 提交到队列执行
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  const VkResult submit_result =
      vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
  if (submit_result != VK_SUCCESS) {
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    LOG_FATAL("[RenderSystem::copyBuffer] vkQueueSubmit failed: {}",
              static_cast<int>(submit_result));
  }

  // 简化处理：直接等待队列空闲，保证拷贝完成后再返回
  const VkResult wait_result = vkQueueWaitIdle(queue);
  if (wait_result != VK_SUCCESS) {
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    LOG_FATAL("[RenderSystem::copyBuffer] vkQueueWaitIdle failed: {}",
              static_cast<int>(wait_result));
  }

  // 释放临时命令缓冲
  vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

// 示例三角形顶点数据（位置 + 颜色）
const Vertex k_triangle_vertices[] = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
};

const uint16_t k_triangle_indices[] = {0, 1, 2};

}  // namespace

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() { shutdown(); }

void RenderSystem::initialize(const RenderSystemInitInfo& info) {
  ASSERT(info.window);

  m_window = info.window;

  // 初始化 Slang 编译器（独立于 Vulkan）
  m_slang_compiler = eastl::make_shared<SlangCompiler>();
  m_slang_compiler->initialize();

  // 初始化 Vulkan 上下文（实例、设备、队列等）
  m_context = eastl::make_shared<VulkanContext>();
  VulkanContextCreateInfo context_info;
  context_info.window = info.window;
  context_info.enable_validation = info.enable_validation;
  m_context->initialize(context_info);

  // 初始化 VMA 分配器
  m_allocator = eastl::make_shared<VulkanAllocator>();
  m_allocator->initialize(m_context.get());

  // 获取窗口 framebuffer 大小并创建交换链
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(info.window, &framebuffer_width, &framebuffer_height);
  ASSERT(framebuffer_width > 0 && framebuffer_height > 0);

  m_swapchain = eastl::make_shared<VulkanSwapchain>();
  m_swapchain->initialize(m_context.get(),
                          static_cast<uint32_t>(framebuffer_width),
                          static_cast<uint32_t>(framebuffer_height));

  // 初始化同步对象（fence/semaphore）
  m_sync = eastl::make_shared<VulkanSync>();
  m_sync->initialize(m_context.get(), m_swapchain->getImageCount());

  // 初始化图形管线（RenderPass、Pipeline、Framebuffer、CommandBuffer）
  m_pipeline = eastl::make_shared<VulkanPipeline>();
  m_pipeline->initialize(m_context.get(), m_swapchain.get(),
                         m_slang_compiler.get());

  // 创建 staging buffer，上传 CPU 顶点数据
  eastl::unique_ptr<VulkanBuffer> staging_buffer =
      eastl::make_unique<VulkanBuffer>();
  staging_buffer->create(m_allocator.get(), sizeof(k_triangle_vertices),
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
  staging_buffer->upload(k_triangle_vertices, sizeof(k_triangle_vertices));

  // 创建 GPU 本地顶点缓冲
  m_vertex_buffer = eastl::make_unique<VulkanBuffer>();
  m_vertex_buffer->create(
      m_allocator.get(), sizeof(k_triangle_vertices),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  // 将 staging buffer 内容拷贝到 GPU 顶点缓冲
  copyBuffer(m_context->getDevice(), m_pipeline->getCommandPool(),
             m_context->getGraphicsQueue(), staging_buffer->getBuffer(),
             m_vertex_buffer->getBuffer(), sizeof(k_triangle_vertices));

  staging_buffer->destroy();

  eastl::unique_ptr<VulkanBuffer> index_staging_buffer =
      eastl::make_unique<VulkanBuffer>();
  index_staging_buffer->create(m_allocator.get(), sizeof(k_triangle_indices),
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VMA_MEMORY_USAGE_CPU_TO_GPU);
  index_staging_buffer->upload(k_triangle_indices, sizeof(k_triangle_indices));

  m_index_buffer = eastl::make_unique<VulkanBuffer>();
  m_index_buffer->create(
      m_allocator.get(), sizeof(k_triangle_indices),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  copyBuffer(m_context->getDevice(), m_pipeline->getCommandPool(),
             m_context->getGraphicsQueue(), index_staging_buffer->getBuffer(),
             m_index_buffer->getBuffer(), sizeof(k_triangle_indices));

  index_staging_buffer->destroy();

  // 创建统一缓冲区
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
}

void RenderSystem::shutdown() {
  if (!m_context) {
    return;
  }

  // 关闭前等待 GPU 全部工作完成，避免资源在使用中被销毁
  vkDeviceWaitIdle(m_context->getDevice());

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

  if (m_sync) {
    m_sync->shutdown();
    m_sync.reset();
  }

  if (m_swapchain) {
    m_swapchain->shutdown();
    m_swapchain.reset();
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

  m_window = nullptr;
  m_current_frame = 0;
}

void RenderSystem::tick(float delta_time, void (*overlay_fn)(VkCommandBuffer)) {
  (void)delta_time;

  GlobalUniformData ubo{};
  ubo.model = glm::mat4(1.0f);
  ubo.view =
      glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 1.0f, 0.0f));
  const VkExtent2D extent = m_swapchain->getExtent();
  const float aspect =
      static_cast<float>(extent.width) / static_cast<float>(extent.height);
  ubo.proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
  ubo.proj[1][1] *= -1.0f;

  // 更新统一变量数据
  m_uniform_buffers[m_current_frame]->upload(&ubo, sizeof(ubo));

  VkDevice device = m_context->getDevice();

  // 等待当前 frame 对应的 fence，确保 CPU 不会覆盖 GPU 仍在使用的资源
  VkFence in_flight_fence = m_sync->getInFlightFence(m_current_frame);
  vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE,
                  k_fence_wait_timeout_ns);

  // 从交换链获取下一个可用图像的索引，并在图像可用时通过信号量通知
  uint32_t image_index = 0;
  VkResult acquire_result = vkAcquireNextImageKHR(
      device, m_swapchain->getSwapchain(), k_fence_wait_timeout_ns,
      m_sync->getImageAvailableSemaphore(m_current_frame), VK_NULL_HANDLE,
      &image_index);

  if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain();
    return;
  }

  // 如果交换链图像正在被另一个帧使用，等待其 fence 完成后再继续
  VkFence image_in_flight_fence = m_sync->getImageInFlightFence(image_index);
  if (image_in_flight_fence != VK_NULL_HANDLE) {
    vkWaitForFences(device, 1, &image_in_flight_fence, VK_TRUE, UINT64_MAX);
  }

  // 当前 frame fence 复位，供本次提交使用
  vkResetFences(device, 1, &in_flight_fence);

  // 复用命令缓冲，先重置后重新录制
  VkCommandBuffer command_buffer =
      m_pipeline->getCommandBuffer(m_current_frame);
  vkResetCommandBuffer(command_buffer, 0);

  // 开始记录命令缓冲区
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(command_buffer, &begin_info);

  VkClearValue clear_color{};
  clear_color.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

  // 开始渲染通道
  VkRenderPassBeginInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = m_pipeline->getRenderPass();
  render_pass_info.framebuffer = m_pipeline->getFramebuffer(image_index);
  render_pass_info.renderArea.offset = {0, 0};
  render_pass_info.renderArea.extent = m_swapchain->getExtent();
  render_pass_info.clearValueCount = 1;
  render_pass_info.pClearValues = &clear_color;

  vkCmdBeginRenderPass(command_buffer, &render_pass_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  // 绑定图形管线
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline->getGraphicsPipeline());
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline->getPipelineLayout(), 0, 1,
                          &m_descriptor_sets[m_current_frame], 0, nullptr);

  // 视口:描述输出将要渲染到的帧缓冲区区域
  VkViewport viewport{};
  viewport.width = static_cast<float>(m_swapchain->getExtent().width);
  viewport.height = static_cast<float>(m_swapchain->getExtent().height);
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  // 裁剪区域与交换链图像大小一致
  VkRect2D scissor{};
  scissor.extent = m_swapchain->getExtent();
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  // 绑定顶点缓冲并绘制三角形
  VkBuffer vertex_buffers[] = {m_vertex_buffer->getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
  vkCmdBindIndexBuffer(command_buffer, m_index_buffer->getBuffer(), 0,
                       VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(command_buffer,
                   static_cast<uint32_t>(sizeof(k_triangle_indices) /
                                         sizeof(k_triangle_indices[0])),
                   1, 0, 0, 0);

  // 可选叠加绘制（如 ImGui）
  if (overlay_fn) {
    overlay_fn(command_buffer);
  }

  // 结束 render pass 与命令录制
  vkCmdEndRenderPass(command_buffer);
  vkEndCommandBuffer(command_buffer);

  // 提交依赖：等待图像可用 -> 执行绘制 -> 发出渲染完成信号
  VkSemaphore wait_semaphores[] = {
      m_sync->getImageAvailableSemaphore(m_current_frame)};
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore signal_semaphores[] = {
      m_sync->getRenderFinishedSemaphore(image_index)};

  // 提交命令缓冲区到图形队列，并在 GPU 完成时通过 fence 通知
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semaphores;
  vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submit_info,
                in_flight_fence);

  // 标记当前交换链图像正在被当前帧的 fence 使用，以避免在下一帧重用时发生冲突
  m_sync->setImageInFlightFence(image_index, in_flight_fence);

  // 将渲染完成的图像提交到显示队列
  VkSwapchainKHR swapchains[] = {m_swapchain->getSwapchain()};
  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = signal_semaphores;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swapchains;
  present_info.pImageIndices = &image_index;
  VkResult present_result =
      vkQueuePresentKHR(m_context->getPresentQueue(), &present_info);
  if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
      present_result == VK_SUBOPTIMAL_KHR) {
    recreateSwapchain();
  }

  // 轮转到下一帧（帧资源循环复用）
  m_current_frame = (m_current_frame + 1) % VulkanSync::k_max_frames_in_flight;
}

void RenderSystem::recreateSwapchain() {
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
  if (framebuffer_width <= 0 || framebuffer_height <= 0) {
    // 窗口最小化等场景，跳过本次重建
    return;
  }

  // 重建前等待设备空闲，确保旧交换链资源不再被 GPU 使用
  vkDeviceWaitIdle(m_context->getDevice());
  m_swapchain->resize(static_cast<uint32_t>(framebuffer_width),
                      static_cast<uint32_t>(framebuffer_height));
  m_sync->recreateRenderFinishedSemaphores(m_swapchain->getImageCount());
  m_pipeline->recreateFramebuffers(m_swapchain.get());
}

VkRenderPass RenderSystem::getRenderPass() const {
  return m_pipeline ? m_pipeline->getRenderPass() : VK_NULL_HANDLE;
}

}  // namespace Blunder
