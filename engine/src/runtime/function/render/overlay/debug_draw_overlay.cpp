#include "runtime/function/render/overlay/debug_draw_overlay.h"

#include <algorithm>
#include <cmath>
#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>

#include "EASTL/memory.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/overlay/debug_draw.h"
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

struct DebugDrawUniformData {
  glm::mat4 view{1.0f};
  glm::mat4 proj{1.0f};
  float line_width_px{1.0f};
  float viewport_width_px{1.0f};
  float viewport_height_px{1.0f};
  float command_count{0.0f};
  float alpha_mul{1.0f};
  float pad0{0.0f};
  float pad1{0.0f};
  float pad2{0.0f};
};

static_assert(sizeof(DebugDrawUniformData) == 160u,
              "DebugDrawUniformData must match debug_draw.slang std140 UBO");

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

rhi::GraphicsPipelineDesc makeDebugDrawDesc(rhi::CompareOp depth_compare) {
  rhi::GraphicsPipelineDesc desc{};
  desc.shader_path = "engine/shaders/debug_draw.slang";
  desc.enable_vertex_input = false;
  desc.topology = rhi::PrimitiveTopology::TriangleList;
  desc.cull_mode = rhi::CullMode::None;
  desc.enable_blend = true;
  desc.enable_depth_test = true;
  desc.enable_depth_write = false;
  desc.depth_compare_op = depth_compare;
  desc.expected_descriptor_binding_count = 2;
  desc.expected_descriptor_bindings[0] = 0;
  desc.expected_descriptor_bindings[1] = 1;
  desc.expected_descriptor_kinds[0] = ShaderDescriptorKind::UniformBuffer;
  desc.expected_descriptor_kinds[1] = ShaderDescriptorKind::StorageBuffer;
  return desc;
}

}  // namespace

DebugDrawOverlay::~DebugDrawOverlay() {
  shutdown();
}

void DebugDrawOverlay::initialize(const OverlayResources& res,
                                  SlangCompiler* compiler,
                                  VkRenderPass render_pass) {
  ASSERT(res.vk_context);
  ASSERT(res.vk_allocator);
  ASSERT(compiler);
  ASSERT(render_pass != VK_NULL_HANDLE);

  m_vk_context = res.vk_context;
  m_vk_allocator = res.vk_allocator;

  m_xray_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_xray_pipeline->bind(m_vk_context, compiler);
  m_xray_pipeline->initializeWithRenderPass(
      render_pass, makeDebugDrawDesc(rhi::CompareOp::Greater));

  m_front_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_front_pipeline->bind(m_vk_context, compiler);
  m_front_pipeline->initializeWithRenderPass(
      render_pass, makeDebugDrawDesc(rhi::CompareOp::LessOrEqual));

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  m_uniform_xray.resize(frames);
  m_uniform_front.resize(frames);
  m_storage_buffers.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_uniform_xray[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_xray[i]->create(m_vk_allocator, sizeof(DebugDrawUniformData),
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_uniform_front[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_front[i]->create(m_vk_allocator, sizeof(DebugDrawUniformData),
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
  pool_sizes[0].descriptorCount = frames * 2u;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_sizes[1].descriptorCount = frames * 2u;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = frames * 2u;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  const VkResult pool_result =
      vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
  if (pool_result != VK_SUCCESS) {
    LOG_FATAL("[DebugDrawOverlay] vkCreateDescriptorPool failed: {}",
              static_cast<int>(pool_result));
  }
  m_descriptor_pool = reinterpret_cast<uintptr_t>(pool);

  const VkDescriptorSetLayout layout =
      m_front_pipeline->nativePipeline()->getDescriptorSetLayout();
  eastl::vector<VkDescriptorSetLayout> layouts(frames * 2u, layout);
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = frames * 2u;
  alloc_info.pSetLayouts = layouts.data();

  eastl::vector<VkDescriptorSet> sets(frames * 2u);
  const VkResult set_result =
      vkAllocateDescriptorSets(device, &alloc_info, sets.data());
  if (set_result != VK_SUCCESS) {
    LOG_FATAL("[DebugDrawOverlay] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_sets_xray.resize(frames);
  m_sets_front.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_sets_xray[i] = reinterpret_cast<uintptr_t>(sets[i]);
    m_sets_front[i] = reinterpret_cast<uintptr_t>(sets[frames + i]);
    updateFrameDescriptors(i);
  }
}

void DebugDrawOverlay::shutdown() {
  if (m_vk_context == nullptr) {
    return;
  }
  VkDevice device = m_vk_context->getDevice();
  auto destroy_bufs = [](auto& bufs) {
    for (auto& buf : bufs) {
      if (buf) {
        buf->destroy();
        buf.reset();
      }
    }
    bufs.clear();
  };
  destroy_bufs(m_uniform_xray);
  destroy_bufs(m_uniform_front);
  destroy_bufs(m_storage_buffers);
  m_sets_xray.clear();
  m_sets_front.clear();
  if (m_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        device, reinterpret_cast<VkDescriptorPool>(m_descriptor_pool), nullptr);
    m_descriptor_pool = 0;
  }
  if (m_xray_pipeline) {
    m_xray_pipeline->shutdown();
    m_xray_pipeline.reset();
  }
  if (m_front_pipeline) {
    m_front_pipeline->shutdown();
    m_front_pipeline.reset();
  }
  m_vk_allocator = nullptr;
  m_vk_context = nullptr;
}

void DebugDrawOverlay::updateFrameDescriptors(uint32_t frame_slot) {
  auto write_set = [&](uintptr_t set_bits, VulkanBuffer& ubo) {
    const VkDescriptorSet set = reinterpret_cast<VkDescriptorSet>(set_bits);
    VkDescriptorBufferInfo ubo_info{};
    ubo_info.buffer = ubo.getBuffer();
    ubo_info.offset = 0;
    ubo_info.range = sizeof(DebugDrawUniformData);
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
  };
  write_set(m_sets_xray[frame_slot], *m_uniform_xray[frame_slot]);
  write_set(m_sets_front[frame_slot], *m_uniform_front[frame_slot]);
}

void DebugDrawOverlay::ensureStorageCapacity(uint32_t frame_slot,
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
  updateFrameDescriptors(frame_slot);
}

DebugDrawBatchStats DebugDrawOverlay::draw(VkCommandBuffer cmd,
                                           const OverlayState& state) {
  DebugDrawBatchStats stats{};
  if (m_front_pipeline == nullptr || m_xray_pipeline == nullptr) {
    return stats;
  }

  m_stream.clear();
  DebugDraw::expandLines(m_stream, state);
  stats.command_count = m_stream.size();
  if (stats.command_count == 0u) {
    return stats;
  }

  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  const uint32_t frame_slot = state.frame_index % frames;

  DebugDrawUniformData uniform{};
  uniform.view = state.view;
  uniform.proj = state.projection;
  uniform.line_width_px = DebugDraw::k_default_width_px;
  uniform.viewport_width_px =
      std::max(static_cast<float>(state.viewport_width), 1.0f);
  uniform.viewport_height_px =
      std::max(static_cast<float>(state.viewport_height), 1.0f);
  uniform.command_count = static_cast<float>(stats.command_count);

  ensureStorageCapacity(frame_slot, stats.command_count);
  m_storage_buffers[frame_slot]->upload(
      m_stream.commands().data(),
      static_cast<VkDeviceSize>(stats.command_count * sizeof(GizmoLineCommand)));

  uniform.alpha_mul = DebugDraw::k_xray_alpha;
  m_uniform_xray[frame_slot]->upload(&uniform, sizeof(uniform));
  uniform.alpha_mul = 1.0f;
  m_uniform_front[frame_slot]->upload(&uniform, sizeof(uniform));

  auto bind_draw = [&](vulkan_backend::VulkanGraphicsPipeline& pipeline,
                       uintptr_t set_bits) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline.nativePipeline()->getGraphicsPipeline());
    const VkDescriptorSet descriptor_set =
        reinterpret_cast<VkDescriptorSet>(set_bits);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.nativePipeline()->getPipelineLayout(), 0,
                            1, &descriptor_set, 0, nullptr);
    vkCmdDraw(cmd, k_gizmo_line_batch_verts, stats.command_count, 0, 0);
  };

  bind_draw(*m_xray_pipeline, m_sets_xray[frame_slot]);
  bind_draw(*m_front_pipeline, m_sets_front[frame_slot]);
  stats.draw_calls = 2;
  return stats;
}

}  // namespace Blunder
