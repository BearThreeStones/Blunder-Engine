#include "runtime/function/render/overlay/gizmo_line_batch.h"

#include <algorithm>
#include <cmath>
#include <vulkan/vulkan.h>

#include "EASTL/memory.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/slang/shader_resource_layout.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"

namespace Blunder {

namespace {

constexpr uint32_t k_min_storage_commands = 256u;

struct GizmoLineUniformData {
  glm::mat4 view{1.0f};
  glm::mat4 proj{1.0f};
  float line_width_px{0.0f};
  float viewport_width_px{1.0f};
  float viewport_height_px{1.0f};
  float command_count{0.0f};
};

static_assert(sizeof(GizmoLineUniformData) == 144u,
              "GizmoLineUniformData must match camera_gizmo.slang std140 UBO");

uint32_t nextPow2(uint32_t value) {
  uint32_t cap = k_min_storage_commands;
  while (cap < value) {
    if (cap > (1u << 30)) {
      return value;
    }
    cap *= 2u;
  }
  return cap;
}

}  // namespace

GizmoLineBatch::~GizmoLineBatch() {
  shutdown();
}

void GizmoLineBatch::initialize(const OverlayResources& res,
                                SlangCompiler* compiler) {
  ASSERT(res.vk_context);
  ASSERT(res.vk_allocator);
  ASSERT(compiler);
  ASSERT(res.screen_render_pass != VK_NULL_HANDLE);

  m_vk_context = res.vk_context;
  m_vk_allocator = res.vk_allocator;

  rhi::GraphicsPipelineDesc desc{};
  desc.shader_path = "engine/shaders/camera_gizmo.slang";
  desc.enable_vertex_input = false;
  desc.topology = rhi::PrimitiveTopology::TriangleList;
  desc.cull_mode = rhi::CullMode::None;
  desc.enable_blend = true;
  desc.enable_depth_test = false;
  desc.enable_depth_write = false;
  desc.expected_descriptor_binding_count = 2;
  desc.expected_descriptor_bindings[0] = 0;
  desc.expected_descriptor_bindings[1] = 1;
  desc.expected_descriptor_kinds[0] = ShaderDescriptorKind::UniformBuffer;
  desc.expected_descriptor_kinds[1] = ShaderDescriptorKind::StorageBuffer;

  m_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_pipeline->bind(m_vk_context, compiler);
  m_pipeline->initializeWithRenderPass(res.screen_render_pass, desc);

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  m_uniform_buffers.resize(frames);
  m_storage_buffers.resize(frames);
  m_uploaded_hash.assign(frames, 0ull);
  for (uint32_t i = 0; i < frames; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(GizmoLineUniformData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_storage_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_storage_buffers[i]->create(
        m_vk_allocator,
        static_cast<VkDeviceSize>(k_min_storage_commands *
                                  sizeof(GizmoLineCommand)),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  VkDevice device = m_vk_context->getDevice();
  VkDescriptorPoolSize pool_sizes[2]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = frames;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_sizes[1].descriptorCount = frames;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = frames;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  const VkResult pool_result =
      vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
  if (pool_result != VK_SUCCESS) {
    LOG_FATAL("[GizmoLineBatch] vkCreateDescriptorPool failed: {}",
              static_cast<int>(pool_result));
  }
  m_descriptor_pool = reinterpret_cast<uintptr_t>(pool);

  const VkDescriptorSetLayout layout =
      m_pipeline->nativePipeline()->getDescriptorSetLayout();
  eastl::vector<VkDescriptorSetLayout> layouts(frames, layout);
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = frames;
  alloc_info.pSetLayouts = layouts.data();

  eastl::vector<VkDescriptorSet> sets(frames);
  const VkResult set_result =
      vkAllocateDescriptorSets(device, &alloc_info, sets.data());
  if (set_result != VK_SUCCESS) {
    LOG_FATAL("[GizmoLineBatch] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_descriptor_sets.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_descriptor_sets[i] = reinterpret_cast<uintptr_t>(sets[i]);
    updateFrameDescriptors(i);
  }
}

void GizmoLineBatch::shutdown() {
  if (m_vk_context == nullptr) {
    return;
  }

  VkDevice device = m_vk_context->getDevice();
  for (auto& buf : m_uniform_buffers) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_uniform_buffers.clear();
  for (auto& buf : m_storage_buffers) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_storage_buffers.clear();
  m_descriptor_sets.clear();
  m_uploaded_hash.clear();
  if (m_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        device, reinterpret_cast<VkDescriptorPool>(m_descriptor_pool), nullptr);
    m_descriptor_pool = 0;
  }
  if (m_pipeline) {
    m_pipeline->shutdown();
    m_pipeline.reset();
  }
  m_vk_allocator = nullptr;
  m_vk_context = nullptr;
}

void GizmoLineBatch::begin() {
  m_stream.clear();
}

void GizmoLineBatch::push(GizmoLineDrawStyle style, const glm::vec3& p0,
                          const glm::vec3& p1, const glm::vec3& p2,
                          const glm::vec4& color, float width_px) {
  m_stream.push(style, p0, p1, p2, color, width_px);
}

void GizmoLineBatch::updateFrameDescriptors(uint32_t frame_slot) {
  ASSERT(frame_slot < m_descriptor_sets.size());
  const VkDescriptorSet set =
      reinterpret_cast<VkDescriptorSet>(m_descriptor_sets[frame_slot]);

  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = m_uniform_buffers[frame_slot]->getBuffer();
  ubo_info.offset = 0;
  ubo_info.range = sizeof(GizmoLineUniformData);

  VkDescriptorBufferInfo ssbo_info{};
  ssbo_info.buffer = m_storage_buffers[frame_slot]->getBuffer();
  ssbo_info.offset = 0;
  ssbo_info.range = m_storage_buffers[frame_slot]->getSize();

  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = set;
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &ubo_info;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = set;
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].descriptorCount = 1;
  writes[1].pBufferInfo = &ssbo_info;

  vkUpdateDescriptorSets(m_vk_context->getDevice(), 2, writes, 0, nullptr);
}

void GizmoLineBatch::ensureStorageCapacity(uint32_t frame_slot,
                                           uint32_t command_count) {
  const uint32_t needed = std::max(command_count, 1u);
  const VkDeviceSize bytes =
      static_cast<VkDeviceSize>(needed * sizeof(GizmoLineCommand));
  if (m_storage_buffers[frame_slot] &&
      m_storage_buffers[frame_slot]->getSize() >= bytes) {
    return;
  }
  const uint32_t cap = nextPow2(needed);
  m_storage_buffers[frame_slot]->destroy();
  m_storage_buffers[frame_slot]->create(
      m_vk_allocator,
      static_cast<VkDeviceSize>(cap * sizeof(GizmoLineCommand)),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_uploaded_hash[frame_slot] = 0;
  updateFrameDescriptors(frame_slot);
}

GizmoLineBatchStats GizmoLineBatch::flush(VkCommandBuffer cmd,
                                          const OverlayState& state,
                                          float line_width_px) {
  GizmoLineBatchStats stats{};
  stats.command_count = m_stream.size();
  if (m_pipeline == nullptr || stats.command_count == 0u) {
    return stats;
  }

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  const uint32_t frame_slot = state.frame_index % frames;
  const uint64_t hash = m_stream.hash();

  GizmoLineUniformData uniform{};
  uniform.view = state.view;
  uniform.proj = state.projection;
  uniform.line_width_px = line_width_px;
  uniform.viewport_width_px =
      std::max(static_cast<float>(state.viewport_width), 1.0f);
  uniform.viewport_height_px =
      std::max(static_cast<float>(state.viewport_height), 1.0f);
  uniform.command_count = static_cast<float>(stats.command_count);
  m_uniform_buffers[frame_slot]->upload(&uniform, sizeof(uniform));

  ensureStorageCapacity(frame_slot, stats.command_count);
  if (m_uploaded_hash[frame_slot] == hash) {
    stats.gpu_upload_skipped = true;
  } else {
    m_storage_buffers[frame_slot]->upload(
        m_stream.commands().data(),
        static_cast<VkDeviceSize>(stats.command_count * sizeof(GizmoLineCommand)));
    m_uploaded_hash[frame_slot] = hash;
  }

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline->nativePipeline()->getGraphicsPipeline());
  const VkDescriptorSet descriptor_set =
      reinterpret_cast<VkDescriptorSet>(m_descriptor_sets[frame_slot]);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline->nativePipeline()->getPipelineLayout(), 0,
                          1, &descriptor_set, 0, nullptr);
  vkCmdDraw(cmd, k_gizmo_line_batch_verts, stats.command_count, 0, 0);
  stats.draw_calls = 1;
  return stats;
}

}  // namespace Blunder
