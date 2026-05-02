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
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/render/vulkan/vulkan_swapchain.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

namespace {

// fence 等待超时时间（1s）
const uint64_t k_fence_wait_timeout_ns = 1000000000ULL;

// UBO (统一缓冲对象)
struct GlobalUniformData {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};

// 一次性提交的 Buffer 拷贝工具函数：用于将 staging buffer 数据拧贝到 GPU 本地
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

// 示例矩形顶点数据（位置 + 颜色）
const Vertex k_quad_vertices[] = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                  {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                  {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                  {{-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}}};

const uint16_t k_quad_indices[] = {0, 1, 2, 2, 3, 0};

const char* k_ui_overlay_shader_path = "engine/shaders/ui_overlay.slang";

void cmdUiImageBarrier(VkCommandBuffer cmd, VkImage image,
                       VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkAccessFlags src_access = 0;
  VkAccessFlags dst_access = 0;

  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
    dst_access = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    src_access = VK_ACCESS_SHADER_READ_BIT;
    dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
    src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }

  barrier.srcAccessMask = src_access;
  barrier.dstAccessMask = dst_access;

  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

void writeUiDescriptorSet(VkDevice device, VkDescriptorSet set,
                          VkImageView image_view, VkSampler sampler) {
  VkDescriptorImageInfo image_info{};
  image_info.imageView = image_view;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo sampler_info{};
  sampler_info.sampler = sampler;

  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = set;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[0].pImageInfo = &image_info;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = set;
  writes[1].dstBinding = 1;
  writes[1].dstArrayElement = 0;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[1].pImageInfo = &sampler_info;

  vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
}

}  // namespace

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() { shutdown(); }

void RenderSystem::initialize(const RenderSystemInitInfo& info) {
  ASSERT(info.window_system);

  m_window_system = info.window_system;

  // 初始化 Slang 编译器（独立于 Vulkan）
  m_slang_compiler = eastl::make_shared<SlangCompiler>();
  m_slang_compiler->initialize();

  // 初始化 Vulkan 上下文（实例、设备、队列等）
  m_context = eastl::make_shared<VulkanContext>();
  VulkanContextCreateInfo context_info;
  context_info.window_system = info.window_system;
  context_info.enable_validation = info.enable_validation;
  m_context->initialize(context_info);

  // 初始化 VMA 分配器
  m_allocator = eastl::make_shared<VulkanAllocator>();
  m_allocator->initialize(m_context.get());

  // 获取窗口 framebuffer 大小并创建交换链
  eastl::array<int, 2> framebuffer_size = info.window_system->getDrawableSize();
  int framebuffer_width = framebuffer_size[0];
  int framebuffer_height = framebuffer_size[1];
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

  m_editor_camera = eastl::make_unique<EditorCamera>(m_window_system);

  // 创建 staging buffer，上传 CPU 顶点数据
  eastl::unique_ptr<VulkanBuffer> staging_buffer =
      eastl::make_unique<VulkanBuffer>();
  staging_buffer->create(m_allocator.get(), sizeof(k_quad_vertices),
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
  staging_buffer->upload(k_quad_vertices, sizeof(k_quad_vertices));

  // 创建 GPU 本地顶点缓冲
  m_vertex_buffer = eastl::make_unique<VulkanBuffer>();
  m_vertex_buffer->create(
      m_allocator.get(), sizeof(k_quad_vertices),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  // 将 staging buffer 内容拷贝到 GPU 顶点缓冲
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

  // 创建统一缓冲区
  m_uniform_buffers.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_allocator.get(), sizeof(GlobalUniformData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  // 创建描述符池
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

  // 创建描述符集
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

  createUiOverlayResources(m_swapchain->getExtent());
}

void RenderSystem::destroyUiTextureOnly() {
  if (!m_context || !m_allocator) {
    return;
  }
  VkDevice device = m_context->getDevice();
  for (eastl::unique_ptr<VulkanBuffer>& sb : m_ui_staging_buffers) {
    if (sb) {
      sb->destroy();
      sb.reset();
    }
  }
  m_ui_staging_buffers.clear();
  if (m_ui_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, m_ui_image_view, nullptr);
    m_ui_image_view = VK_NULL_HANDLE;
  }
  if (m_ui_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), m_ui_image, m_ui_image_allocation);
    m_ui_image = VK_NULL_HANDLE;
    m_ui_image_allocation = VK_NULL_HANDLE;
  }
  m_ui_alloc_width = 0;
  m_ui_alloc_height = 0;
  m_ui_first_transfer = true;
}

void RenderSystem::ensureUiTexture(VkExtent2D extent) {
  if (!m_allocator || extent.width == 0 || extent.height == 0) {
    return;
  }
  if (m_ui_image != VK_NULL_HANDLE && m_ui_alloc_width == extent.width &&
      m_ui_alloc_height == extent.height) {
    return;
  }

  VkDevice device = m_context->getDevice();
  destroyUiTextureOnly();

  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_info.extent.width = extent.width;
  image_info.extent.height = extent.height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_create{};
  alloc_create.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  const VkResult img_result = vmaCreateImage(
      m_allocator->getAllocator(), &image_info, &alloc_create, &m_ui_image,
      &m_ui_image_allocation, nullptr);
  if (img_result != VK_SUCCESS) {
    LOG_FATAL("[RenderSystem::ensureUiTexture] vmaCreateImage failed: {}",
              static_cast<int>(img_result));
  }

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = m_ui_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  const VkResult view_result =
      vkCreateImageView(device, &view_info, nullptr, &m_ui_image_view);
  if (view_result != VK_SUCCESS) {
    LOG_FATAL("[RenderSystem::ensureUiTexture] vkCreateImageView failed: {}",
              static_cast<int>(view_result));
  }

  const VkDeviceSize staging_bytes =
      static_cast<VkDeviceSize>(extent.width) * extent.height * 4;
  m_ui_staging_buffers.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_ui_staging_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_ui_staging_buffers[i]->create(m_allocator.get(), staging_bytes,
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  m_ui_alloc_width = extent.width;
  m_ui_alloc_height = extent.height;
  m_ui_first_transfer = true;

  if (m_ui_descriptor_set != VK_NULL_HANDLE && m_ui_sampler != VK_NULL_HANDLE) {
    writeUiDescriptorSet(device, m_ui_descriptor_set, m_ui_image_view, m_ui_sampler);
  }
}

void RenderSystem::destroyUiOverlayResources() {
  if (!m_context) {
    return;
  }
  VkDevice device = m_context->getDevice();
  destroyUiTextureOnly();

  if (m_ui_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, m_ui_descriptor_pool, nullptr);
    m_ui_descriptor_pool = VK_NULL_HANDLE;
    m_ui_descriptor_set = VK_NULL_HANDLE;
  }

  if (m_ui_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_ui_pipeline, nullptr);
    m_ui_pipeline = VK_NULL_HANDLE;
  }
  if (m_ui_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_ui_pipeline_layout, nullptr);
    m_ui_pipeline_layout = VK_NULL_HANDLE;
  }
  if (m_ui_descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_ui_descriptor_set_layout, nullptr);
    m_ui_descriptor_set_layout = VK_NULL_HANDLE;
  }
  if (m_ui_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_ui_sampler, nullptr);
    m_ui_sampler = VK_NULL_HANDLE;
  }
}

void RenderSystem::createUiOverlayResources(VkExtent2D extent) {
  ASSERT(m_context);
  ASSERT(m_allocator);
  ASSERT(m_slang_compiler);
  ASSERT(m_pipeline);

  VkDevice device = m_context->getDevice();

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  const VkResult sr =
      vkCreateSampler(device, &sampler_info, nullptr, &m_ui_sampler);
  if (sr != VK_SUCCESS) {
    LOG_FATAL("[RenderSystem::createUiOverlayResources] vkCreateSampler failed: {}",
              static_cast<int>(sr));
  }

  VkDescriptorSetLayoutBinding bindings[2]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo dsl{};
  dsl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  dsl.bindingCount = 2;
  dsl.pBindings = bindings;

  const VkResult dsl_r =
      vkCreateDescriptorSetLayout(device, &dsl, nullptr, &m_ui_descriptor_set_layout);
  if (dsl_r != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::createUiOverlayResources] vkCreateDescriptorSetLayout "
        "failed: {}",
        static_cast<int>(dsl_r));
  }

  VkPipelineLayoutCreateInfo pl{};
  pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pl.setLayoutCount = 1;
  pl.pSetLayouts = &m_ui_descriptor_set_layout;

  const VkResult pl_r =
      vkCreatePipelineLayout(device, &pl, nullptr, &m_ui_pipeline_layout);
  if (pl_r != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::createUiOverlayResources] vkCreatePipelineLayout failed: {}",
        static_cast<int>(pl_r));
  }

  eastl::vector<VulkanShader::EntryPointSpec> ui_entries;
  ui_entries.push_back(
      {"uiVertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  ui_entries.push_back(
      {"uiFragmentMain", VK_SHADER_STAGE_FRAGMENT_BIT, SLANG_STAGE_FRAGMENT});

  eastl::vector<VulkanShader::ShaderStage> ui_shader_stages =
      VulkanShader::loadFromSlang(device, m_slang_compiler.get(),
                                  k_ui_overlay_shader_path, ui_entries);

  eastl::vector<VkPipelineShaderStageCreateInfo> ui_stage_infos;
  ui_stage_infos.reserve(ui_shader_stages.size());
  for (VulkanShader::ShaderStage& stage : ui_shader_stages) {
    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = stage.stage_flags;
    stage_info.module = stage.module;
    stage_info.pName = stage.entry_point.c_str();
    ui_stage_infos.push_back(stage_info);
  }

  VkPipelineVertexInputStateCreateInfo vertex_input_info{};
  vertex_input_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blend_attachment{};
  blend_attachment.blendEnable = VK_TRUE;
  blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &blend_attachment;

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dynamic_states;

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<uint32_t>(ui_stage_infos.size());
  pipeline_info.pStages = ui_stage_infos.data();
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = m_ui_pipeline_layout;
  pipeline_info.renderPass = m_pipeline->getRenderPass();
  pipeline_info.subpass = 0;

  const VkResult pipe_r =
      vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info,
                                nullptr, &m_ui_pipeline);

  for (VulkanShader::ShaderStage& stage : ui_shader_stages) {
    VulkanShader::destroyShaderModule(device, &stage.module);
  }

  if (pipe_r != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::createUiOverlayResources] vkCreateGraphicsPipelines "
        "failed: {}",
        static_cast<int>(pipe_r));
  }

  VkDescriptorPoolSize pool_sizes[2]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  pool_sizes[0].descriptorCount = 1;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  pool_sizes[1].descriptorCount = 1;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;

  const VkResult pool_r =
      vkCreateDescriptorPool(device, &pool_info, nullptr, &m_ui_descriptor_pool);
  if (pool_r != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::createUiOverlayResources] vkCreateDescriptorPool failed: {}",
        static_cast<int>(pool_r));
  }

  VkDescriptorSetAllocateInfo ds_alloc{};
  ds_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ds_alloc.descriptorPool = m_ui_descriptor_pool;
  ds_alloc.descriptorSetCount = 1;
  ds_alloc.pSetLayouts = &m_ui_descriptor_set_layout;

  const VkResult ds_r =
      vkAllocateDescriptorSets(device, &ds_alloc, &m_ui_descriptor_set);
  if (ds_r != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::createUiOverlayResources] vkAllocateDescriptorSets failed: {}",
        static_cast<int>(ds_r));
  }

  ensureUiTexture(extent);
}

void RenderSystem::shutdown() {
  if (!m_context) {
    return;
  }

  // 关闭前等待 GPU 全部工作完成，避免资源在使用中被销毁
  vkDeviceWaitIdle(m_context->getDevice());

  destroyUiOverlayResources();

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

  m_window_system = nullptr;
  m_current_frame = 0;
}

void RenderSystem::tick(float delta_time, const UiCpuTextureView* ui_overlay,
                        void (*overlay_fn)(VkCommandBuffer)) {
  m_elapsed_time += delta_time;

  GlobalUniformData ubo{};

  // 模型矩阵：围绕 Z 轴旋转，每秒旋转 90 度
  ubo.model = glm::rotate(glm::mat4(1.0f), m_elapsed_time * glm::radians(90.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));

  const VkExtent2D extent = m_swapchain->getExtent();

  const bool draw_ui =
      ui_overlay && ui_overlay->pixels_rgba && m_ui_pipeline != VK_NULL_HANDLE &&
      m_current_frame < m_ui_staging_buffers.size() &&
      m_ui_staging_buffers[m_current_frame] && ui_overlay->width == extent.width &&
      ui_overlay->height == extent.height;
  if (m_editor_camera) {
    m_editor_camera->setViewportSize(static_cast<float>(extent.width),
                                     static_cast<float>(extent.height));
    m_editor_camera->onUpdate(delta_time);
    ubo.view = m_editor_camera->getViewMatrix();
    ubo.projection = m_editor_camera->getProjectionMatrix();
  } else {
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                           glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));

    const float aspect =
        static_cast<float>(extent.width) / static_cast<float>(extent.height);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
    proj[1][1] *= -1.0f;
    ubo.projection = proj;
  }

  // 更新统一变量数据
  m_uniform_buffers[m_current_frame]->upload(&ubo, sizeof(ubo));

  VkDevice device = m_context->getDevice();

  // 等待当前 frame 对应的 fence，确保 CPU 不会覆盖 GPU 仍在使用的资源
  VkFence in_flight_fence = m_sync->getInFlightFence(m_current_frame);
  vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE,
                  k_fence_wait_timeout_ns);

  if (draw_ui) {
    const VkDeviceSize nbytes =
        static_cast<VkDeviceSize>(extent.width) * extent.height * 4;
    m_ui_staging_buffers[m_current_frame]->upload(ui_overlay->pixels_rgba,
                                                  nbytes);
  }

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

  if (draw_ui && m_ui_image != VK_NULL_HANDLE) {
    const VkImageLayout old_layout = m_ui_first_transfer
                                       ? VK_IMAGE_LAYOUT_UNDEFINED
                                       : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    cmdUiImageBarrier(command_buffer, m_ui_image, old_layout,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent.width = extent.width;
    region.imageExtent.height = extent.height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(
        command_buffer, m_ui_staging_buffers[m_current_frame]->getBuffer(),
        m_ui_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    cmdUiImageBarrier(command_buffer, m_ui_image,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_ui_first_transfer = false;
  }

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

  // 使用描述符集绑定统一缓冲区数据
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

  // 绑定顶点缓冲并绘制矩形
  VkBuffer vertex_buffers[] = {m_vertex_buffer->getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
  vkCmdBindIndexBuffer(command_buffer, m_index_buffer->getBuffer(), 0,
                       VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(
      command_buffer,
      static_cast<uint32_t>(sizeof(k_quad_indices) / sizeof(k_quad_indices[0])),
      1, 0, 0, 0);

  if (draw_ui) {
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_ui_pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_ui_pipeline_layout, 0, 1, &m_ui_descriptor_set, 0,
                            nullptr);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_pipeline->getGraphicsPipeline());
  }

  // 可选叠加绘制（如调试 overlay）
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

void RenderSystem::onEvent(Event& event) {
  if (m_editor_camera) {
    m_editor_camera->onEvent(event);
  }
}

void RenderSystem::recreateSwapchain() {
  eastl::array<int, 2> framebuffer_size = m_window_system->getDrawableSize();
  int framebuffer_width = framebuffer_size[0];
  int framebuffer_height = framebuffer_size[1];
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
  ensureUiTexture(m_swapchain->getExtent());
}

VkRenderPass RenderSystem::getRenderPass() const {
  return m_pipeline ? m_pipeline->getRenderPass() : VK_NULL_HANDLE;
}

}  // namespace Blunder
