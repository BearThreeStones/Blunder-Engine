#include "runtime/function/render/render_system.h"

#include <cstdint>

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

const uint64_t k_fence_wait_timeout_ns = 1000000000ULL;

const Vertex k_triangle_vertices[] = {
    {{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
};

}  // namespace

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() { shutdown(); }

void RenderSystem::initialize(const RenderSystemInitInfo& info) {
  ASSERT(info.window);

  m_window = info.window;

  // Initialize Slang shader compiler (no Vulkan dependency)
  m_slang_compiler = eastl::make_shared<SlangCompiler>();
  m_slang_compiler->initialize();

  m_context = eastl::make_shared<VulkanContext>();
  VulkanContextCreateInfo context_info;
  context_info.window = info.window;
  context_info.enable_validation = info.enable_validation;
  m_context->initialize(context_info);

  m_allocator = eastl::make_shared<VulkanAllocator>();
  m_allocator->initialize(m_context.get());

  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(info.window, &framebuffer_width, &framebuffer_height);
  ASSERT(framebuffer_width > 0 && framebuffer_height > 0);

  m_swapchain = eastl::make_shared<VulkanSwapchain>();
  m_swapchain->initialize(m_context.get(),
                          static_cast<uint32_t>(framebuffer_width),
                          static_cast<uint32_t>(framebuffer_height));

  m_sync = eastl::make_shared<VulkanSync>();
  m_sync->initialize(m_context.get(), m_swapchain->getImageCount());

  m_pipeline = eastl::make_shared<VulkanPipeline>();
  m_pipeline->initialize(m_context.get(), m_swapchain.get(),
                         m_slang_compiler.get());

  m_vertex_buffer = eastl::make_unique<VulkanBuffer>();
  m_vertex_buffer->create(m_allocator.get(), sizeof(k_triangle_vertices),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_vertex_buffer->upload(k_triangle_vertices, sizeof(k_triangle_vertices));
}

void RenderSystem::shutdown() {
  if (!m_context) {
    return;
  }

  vkDeviceWaitIdle(m_context->getDevice());

  if (m_vertex_buffer) {
    m_vertex_buffer->destroy();
    m_vertex_buffer.reset();
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

  VkDevice device = m_context->getDevice();
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

  vkResetFences(device, 1, &in_flight_fence);

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

  // 视口:描述输出将要渲染到的帧缓冲区区域
  VkViewport viewport{};
  viewport.width = static_cast<float>(m_swapchain->getExtent().width);
  viewport.height = static_cast<float>(m_swapchain->getExtent().height);
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(command_buffer, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = m_swapchain->getExtent();
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);

  VkBuffer vertex_buffers[] = {m_vertex_buffer->getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
  vkCmdDraw(command_buffer, 3, 1, 0, 0);

  if (overlay_fn) {
    overlay_fn(command_buffer);
  }

  vkCmdEndRenderPass(command_buffer);
  vkEndCommandBuffer(command_buffer);

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

  m_current_frame = (m_current_frame + 1) % VulkanSync::k_max_frames_in_flight;
}

void RenderSystem::recreateSwapchain() {
  int framebuffer_width = 0;
  int framebuffer_height = 0;
  glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
  if (framebuffer_width <= 0 || framebuffer_height <= 0) {
    return;
  }

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
