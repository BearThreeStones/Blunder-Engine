#include "runtime/function/render/pick/pick_broad_phase.h"

#include <slang.h>

#include <glm/glm.hpp>
#include <vk_mem_alloc.h>

#include "EASTL/unique_ptr.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"

namespace Blunder {

namespace {

VkPipeline createComputePipeline(VkDevice device, VkPipelineLayout pipeline_layout,
                                 VkShaderModule shader_module) {
  VkPipelineShaderStageCreateInfo stage_info{};
  stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage_info.module = shader_module;
  stage_info.pName = "main";

  VkComputePipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.stage = stage_info;
  pipeline_info.layout = pipeline_layout;

  VkPipeline pipeline = VK_NULL_HANDLE;
  const VkResult result =
      vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                               &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[PickBroadPhase] vkCreateComputePipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

}  // namespace

PickBroadPhase::~PickBroadPhase() { shutdown(); }

void PickBroadPhase::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                                SlangCompiler* compiler) {
  m_context = ctx;
  m_allocator = alloc;
  m_compiler = compiler;

  if (m_context == nullptr || m_allocator == nullptr || m_compiler == nullptr) {
    return;
  }

  m_uniform_buffer = eastl::make_unique<VulkanBuffer>();
  m_uniform_buffer->create(alloc, sizeof(PickBroadUniforms),
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                           VMA_MEMORY_USAGE_CPU_TO_GPU);

  m_hit_records_buffer = eastl::make_unique<VulkanBuffer>();
  m_hit_records_buffer->create(
      alloc, sizeof(BroadHitGpuRecord) * k_max_hits,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);

  m_hit_count_buffer = eastl::make_unique<VulkanBuffer>();
  m_hit_count_buffer->create(alloc, sizeof(uint32_t),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_GPU_TO_CPU);

  createDescriptorResources();
  createPipeline();
}

void PickBroadPhase::shutdown() {
  destroyPipeline();
  destroyDescriptorResources();

  if (m_hit_count_buffer) {
    m_hit_count_buffer->destroy();
    m_hit_count_buffer.reset();
  }
  if (m_hit_records_buffer) {
    m_hit_records_buffer->destroy();
    m_hit_records_buffer.reset();
  }
  if (m_uniform_buffer) {
    m_uniform_buffer->destroy();
    m_uniform_buffer.reset();
  }

  m_compiler = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
}

void PickBroadPhase::createDescriptorResources() {
  VkDevice device = m_context->getDevice();

  VkDescriptorSetLayoutBinding bindings[4]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  bindings[2].binding = 2;
  bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  bindings[3].binding = 3;
  bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[3].descriptorCount = 1;
  bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 4;
  layout_info.pBindings = bindings;
  vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &m_descriptor_layout);

  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &m_descriptor_layout;
  vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
                         &m_pipeline_layout);

  VkDescriptorPoolSize pool_sizes[2]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = 1;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_sizes[1].descriptorCount = 3;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 2;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = 1;
  vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool);

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = m_descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &m_descriptor_layout;
  vkAllocateDescriptorSets(device, &alloc_info, &m_descriptor_set);

  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = m_uniform_buffer->getBuffer();
  ubo_info.range = sizeof(PickBroadUniforms);

  VkDescriptorBufferInfo hits_info{};
  hits_info.buffer = m_hit_records_buffer->getBuffer();
  hits_info.range = sizeof(BroadHitGpuRecord) * k_max_hits;

  VkDescriptorBufferInfo count_info{};
  count_info.buffer = m_hit_count_buffer->getBuffer();
  count_info.range = sizeof(uint32_t);

  VkWriteDescriptorSet writes[3]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_descriptor_set;
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &ubo_info;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = m_descriptor_set;
  writes[1].dstBinding = 2;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].descriptorCount = 1;
  writes[1].pBufferInfo = &hits_info;

  writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[2].dstSet = m_descriptor_set;
  writes[2].dstBinding = 3;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[2].descriptorCount = 1;
  writes[2].pBufferInfo = &count_info;

  vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
}

void PickBroadPhase::destroyDescriptorResources() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
    m_descriptor_set = VK_NULL_HANDLE;
  }
  if (m_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_pipeline_layout, nullptr);
    m_pipeline_layout = VK_NULL_HANDLE;
  }
  if (m_descriptor_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_descriptor_layout, nullptr);
    m_descriptor_layout = VK_NULL_HANDLE;
  }
}

void PickBroadPhase::updateInstanceBufferDescriptor(VkBuffer instance_buffer) {
  if (m_context == nullptr || instance_buffer == VK_NULL_HANDLE) {
    return;
  }

  VkDescriptorBufferInfo instance_info{};
  instance_info.buffer = instance_buffer;
  instance_info.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = m_descriptor_set;
  write.dstBinding = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  write.descriptorCount = 1;
  write.pBufferInfo = &instance_info;
  vkUpdateDescriptorSets(m_context->getDevice(), 1, &write, 0, nullptr);
}

void PickBroadPhase::createPipeline() {
  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back({"main", VK_SHADER_STAGE_COMPUTE_BIT, SLANG_STAGE_COMPUTE});

  auto stages = VulkanShader::loadFromSlang(
      m_context->getDevice(), m_compiler, "engine/shaders/pick_broad_phase.slang",
      entries);
  m_pipeline = createComputePipeline(m_context->getDevice(), m_pipeline_layout,
                                     stages.front().module);
  for (auto& stage : stages) {
    VulkanShader::destroyShaderModule(m_context->getDevice(), &stage.module);
  }
}

void PickBroadPhase::destroyPipeline() {
  if (m_context != nullptr && m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_context->getDevice(), m_pipeline, nullptr);
    m_pipeline = VK_NULL_HANDLE;
  }
}

void PickBroadPhase::recordDispatch(VkCommandBuffer cmd, const Ray& ray,
                                    VkBuffer instance_buffer,
                                    const uint32_t instance_count) {
  if (m_pipeline == VK_NULL_HANDLE || instance_buffer == VK_NULL_HANDLE ||
      instance_count == 0) {
    return;
  }

  updateInstanceBufferDescriptor(instance_buffer);

  PickBroadUniforms uniforms{};
  uniforms.ray_origin = ray.origin;
  uniforms.ray_dir = ray.direction;
  uniforms.instance_count = instance_count;
  uniforms.max_hits = k_max_hits;
  m_uniform_buffer->upload(&uniforms, sizeof(uniforms));

  const uint32_t zero = 0u;
  m_hit_count_buffer->upload(&zero, sizeof(zero));

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout,
                          0, 1, &m_descriptor_set, 0, nullptr);

  const uint32_t group_count =
      (instance_count + 63u) / 64u;
  vkCmdDispatch(cmd, group_count, 1, 1);
}

void PickBroadPhase::readbackHits(eastl::vector<BroadHit>& out) {
  out.clear();
  if (m_allocator == nullptr || m_hit_count_buffer == nullptr ||
      m_hit_records_buffer == nullptr) {
    return;
  }

  uint32_t hit_count = 0;
  void* count_mapped = nullptr;
  vmaMapMemory(m_allocator->getAllocator(), m_hit_count_buffer->getAllocation(),
               &count_mapped);
  if (count_mapped != nullptr) {
    hit_count = *static_cast<const uint32_t*>(count_mapped);
    vmaUnmapMemory(m_allocator->getAllocator(), m_hit_count_buffer->getAllocation());
  }

  hit_count = eastl::min(hit_count, k_max_hits);
  if (hit_count == 0) {
    return;
  }

  void* hits_mapped = nullptr;
  vmaMapMemory(m_allocator->getAllocator(), m_hit_records_buffer->getAllocation(),
               &hits_mapped);
  if (hits_mapped == nullptr) {
    return;
  }

  const BroadHitGpuRecord* records =
      static_cast<const BroadHitGpuRecord*>(hits_mapped);
  out.reserve(hit_count);
  for (uint32_t i = 0; i < hit_count; ++i) {
    BroadHit hit{};
    hit.entity_id = static_cast<EntityId>(records[i].entity_id);
    hit.t = records[i].t;
    out.push_back(hit);
  }
  vmaUnmapMemory(m_allocator->getAllocator(), m_hit_records_buffer->getAllocation());
}

}  // namespace Blunder
