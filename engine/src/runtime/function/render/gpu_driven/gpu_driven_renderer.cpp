#include "runtime/function/render/gpu_driven/gpu_driven_renderer.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include <glm/common.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <slang.h>
#include <vk_mem_alloc.h>

#include "EASTL/algorithm.h"
#include "EASTL/sort.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/function/render/gpu_driven/gpu_driven_cull.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/shadow/mesh_shadow_system.h"
#include "runtime/function/render/shadow/shadow_map_target.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/bindless_texture_table.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/resource/asset/material_asset.h"

namespace Blunder {

namespace {

struct GpuDrivenGBufferUniformData {
  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 projection{1.0f};
  glm::mat4 normal_matrix{1.0f};
  glm::vec4 base_color_factor{1.0f};
  glm::vec4 material_flags{0.0f};
  glm::vec4 metallic_roughness_factors{0.0f};
  glm::vec4 pbr_texture_flags{0.0f};
  glm::uvec4 bindless_texture_indices{0};
  glm::uvec4 receiver{0};
};

struct GpuDrivenShadowUniformData {
  glm::mat4 model{1.0f};
  glm::mat4 light_view{1.0f};
  glm::mat4 light_projection{1.0f};
};

struct HizUniformData {
  uint32_t src_width{1};
  uint32_t src_height{1};
  uint32_t dst_width{1};
  uint32_t dst_height{1};
  uint32_t src_is_depth{0};
  uint32_t pad0{0};
  uint32_t pad1{0};
  uint32_t pad2{0};
};

VkPipeline createVkComputePipeline(VkDevice device, VkPipelineLayout layout,
                                   VkShaderModule module, const char* entry) {
  VkPipelineShaderStageCreateInfo stage{};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = module;
  stage.pName = entry;

  VkComputePipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  info.stage = stage;
  info.layout = layout;
  VkPipeline pipeline = VK_NULL_HANDLE;
  const VkResult result =
      vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[GpuDrivenRenderer] vkCreateComputePipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

VkDescriptorType descriptorType(ShaderDescriptorKind kind) {
  switch (kind) {
    case ShaderDescriptorKind::UniformBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ShaderDescriptorKind::SampledImage:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case ShaderDescriptorKind::Sampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;
    case ShaderDescriptorKind::StorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderDescriptorKind::StorageImage:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  }
  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

void cmdBufferBarrier(VkCommandBuffer cmd, VkBuffer buffer, VkAccessFlags src,
                      VkAccessFlags dst, VkPipelineStageFlags src_stage,
                      VkPipelineStageFlags dst_stage) {
  VkBufferMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask = src;
  barrier.dstAccessMask = dst;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer = buffer;
  barrier.size = VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 1, &barrier, 0,
                       nullptr);
}

uint64_t hashMix(uint64_t hash, uint64_t value) {
  hash ^= value;
  hash *= 1099511628211ull;
  return hash;
}

uint64_t hashFloatBits(uint64_t hash, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return hashMix(hash, bits);
}

uint64_t hashVec4Bits(uint64_t hash, const glm::vec4& value) {
  hash = hashFloatBits(hash, value.x);
  hash = hashFloatBits(hash, value.y);
  hash = hashFloatBits(hash, value.z);
  return hashFloatBits(hash, value.w);
}

uint64_t hashEvaluatedLights(const EvaluatedLight* lights, uint32_t count) {
  uint64_t hash = 14695981039346656037ull;
  hash = hashMix(hash, count);
  if (lights == nullptr) {
    return hash;
  }
  for (uint32_t i = 0; i < count; ++i) {
    hash = hashMix(hash, static_cast<uint32_t>(lights[i].entity_id));
    hash = hashMix(hash, static_cast<uint32_t>(lights[i].type));
    hash = hashFloatBits(hash, lights[i].color_times_intensity.x);
    hash = hashFloatBits(hash, lights[i].color_times_intensity.y);
    hash = hashFloatBits(hash, lights[i].color_times_intensity.z);
    hash = hashFloatBits(hash, lights[i].range);
  }
  return hash;
}

uint64_t hashDrawIdentity(const GpuDrivenDraw* draws, uint32_t count) {
  uint64_t hash = 14695981039346656037ull;
  hash = hashMix(hash, count);
  if (draws == nullptr) {
    return hash;
  }
  for (uint32_t i = 0; i < count; ++i) {
    const GpuDrivenDraw& draw = draws[i];
    hash = hashMix(hash, reinterpret_cast<uintptr_t>(draw.gpu_mesh));
    hash = hashMix(hash, reinterpret_cast<uintptr_t>(draw.material.get()));
    hash = hashMix(hash, reinterpret_cast<uintptr_t>(draw.base_color_texture));
    hash =
        hashMix(hash, reinterpret_cast<uintptr_t>(draw.metallic_roughness_texture));
    hash = hashMix(hash, reinterpret_cast<uintptr_t>(draw.normal_texture));
    hash = hashMix(hash, reinterpret_cast<uintptr_t>(draw.occlusion_texture));
    hash = hashMix(hash, draw.double_sided ? 1u : 0u);
    hash = hashMix(hash, static_cast<uint32_t>(draw.alpha_mode));
    hash = hashMix(hash, static_cast<uint32_t>(draw.entity_id));
    hash = hashFloatBits(hash, draw.alpha_cutoff);
    if (const MaterialAsset* material = draw.material.get()) {
      hash = hashVec4Bits(hash, material->getBaseColorFactor());
      hash = hashFloatBits(hash, material->getMetallicFactor());
      hash = hashFloatBits(hash, material->getRoughnessFactor());
      hash = hashFloatBits(hash, material->getAlphaCutoff());
      hash = hashMix(hash, static_cast<uint32_t>(material->getAlphaMode()));
      hash = hashMix(hash, material->isUnlit() ? 1u : 0u);
    }
  }
  return hash;
}

uint32_t gpuDrivenLightMask(const ForwardFrameState& frame_state, EntityId entity_id,
                            const EvaluatedLight* scene_lights,
                            uint32_t scene_light_count) {
  if (!frame_state.live_scene_lighting || frame_state.lighting_scene == nullptr) {
    return 1u;
  }
  if (scene_lights == nullptr || scene_light_count == 0) {
    return 0u;
  }
  EvaluatedLight affecting[k_max_evaluated_lights_per_mesh];
  const size_t affecting_count = gatherLightsForMesh(
      *frame_state.lighting_scene, entity_id, affecting,
      k_max_evaluated_lights_per_mesh);
  uint32_t mask = 0;
  for (size_t j = 0; j < affecting_count; ++j) {
    for (uint32_t k = 0; k < scene_light_count; ++k) {
      if (scene_lights[k].entity_id == affecting[j].entity_id) {
        mask |= (1u << k);
        break;
      }
    }
  }
  return mask;
}

uint64_t hashDrawTransforms(const GpuDrivenDraw* draws, uint32_t count) {
  uint64_t hash = 14695981039346656037ull;
  hash = hashMix(hash, count);
  if (draws == nullptr) {
    return hash;
  }
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t* model = reinterpret_cast<const uint32_t*>(&draws[i].model);
    for (uint32_t word = 0; word < 16u; ++word) {
      hash = hashMix(hash, model[word]);
    }
  }
  return hash;
}

void fillInstance(GpuDrivenInstanceGpu& inst, const GpuDrivenDraw& draw,
                  BindlessTextureTable* table, VulkanTexture* fallback,
                  const ForwardFrameState& frame_state) {
  ForwardMeshUniformData ubo{};
  const MaterialAsset* material = draw.material.get();
  applyPbrToMeshUniforms(ubo, material, frame_state.shading, frame_state,
                         material != nullptr ? material->getAlphaMode()
                                             : draw.alpha_mode,
                         material != nullptr ? material->getAlphaCutoff()
                                             : draw.alpha_cutoff,
                         draw.double_sided, draw.entity_id);
  inst.world = draw.model;
  inst.normal_matrix = glm::inverseTranspose(glm::mat4(glm::mat3(draw.model)));
  auto bindless_index = [&](VulkanTexture* texture) -> uint32_t {
    if (table == nullptr || texture == nullptr || texture == fallback) {
      return BindlessTextureIndexTable::k_fallback_index;
    }
    return table->acquire(texture);
  };
  inst.bindless_texture_indices = glm::uvec4(
      bindless_index(draw.base_color_texture),
      bindless_index(draw.metallic_roughness_texture),
      bindless_index(draw.normal_texture), bindless_index(draw.occlusion_texture));
  inst.base_color_factor = ubo.base_color_factor;
  inst.metallic_roughness_factors = ubo.metallic_roughness_factors;
  inst.pbr_texture_flags = ubo.pbr_texture_flags;
  inst.material_flags = ubo.material_flags;
  // Bindless 0 is the checker fallback, not a PBR map. Keep material_flags.z
  // (roughness-only) so the shader still skips ORM B-as-metal.
  inst.pbr_texture_flags.x =
      inst.bindless_texture_indices.y != 0 ? 1.0f : 0.0f;
  inst.pbr_texture_flags.y =
      inst.bindless_texture_indices.z != 0 ? 1.0f : 0.0f;
  inst.pbr_texture_flags.z =
      inst.bindless_texture_indices.w != 0 ? 1.0f : 0.0f;
  inst.receiver_id = draw.receiver_id;
  inst.flags = draw.double_sided ? k_gpu_driven_flag_two_sided : 0u;
}

}  // namespace

GpuDrivenRenderer::~GpuDrivenRenderer() { shutdown(); }

void GpuDrivenRenderer::initialize(VulkanContext* context, VulkanAllocator* allocator,
                                   SlangCompiler* compiler,
                                   VkRenderPass offscreen_pass,
                                   VkRenderPass shadow_pass,
                                   VkRenderPass gbuffer_pass) {
  m_context = context;
  m_allocator = allocator;
  m_compiler = compiler;
  if (m_context == nullptr || m_allocator == nullptr || m_compiler == nullptr ||
      offscreen_pass == VK_NULL_HANDLE) {
    return;
  }
  m_mesh_shaders_enabled = m_context->meshShadersEnabled();
  createBuffers();
  createPipelines(offscreen_pass, shadow_pass, gbuffer_pass);
  createDescriptors();
  createHiz(1, 1);
}

void GpuDrivenRenderer::shutdown() {
  destroyHiz();
  destroyDescriptors();
  destroyPipelines();
  destroyBuffers();
  m_compiler = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
}

void GpuDrivenRenderer::createBuffers() {
  auto make_host_buf = [this](VkDeviceSize size, VkBufferUsageFlags usage) {
    auto buf = eastl::make_unique<VulkanBuffer>();
    buf->create(m_allocator, size, usage, VMA_MEMORY_USAGE_CPU_TO_GPU);
    return buf;
  };
  auto make_device_buf = [this](VkDeviceSize size, VkBufferUsageFlags usage) {
    auto buf = eastl::make_unique<VulkanBuffer>();
    buf->create(m_allocator, size, usage, VMA_MEMORY_USAGE_GPU_ONLY);
    return buf;
  };
  for (uint32_t i = 0; i < k_frames; ++i) {
    FrameBuffers& frame = m_frames[i];
    frame.instances =
        make_host_buf(sizeof(GpuDrivenInstanceGpu) * k_max_gpu_driven_instances,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.meshlets =
        make_host_buf(sizeof(GpuDrivenMeshletGpu) * k_max_gpu_driven_meshlets,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const VkBufferUsageFlags cmd_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkBufferUsageFlags count_usage =
        cmd_usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const VkDeviceSize cmd_bytes =
        sizeof(DrawIndexedIndirectCommand) * k_max_gpu_driven_meshlets;
    frame.early_cmds = make_device_buf(cmd_bytes, cmd_usage);
    frame.late_cmds = make_device_buf(cmd_bytes, cmd_usage);
    frame.shadow_cmds = make_device_buf(cmd_bytes, cmd_usage);
    frame.dummy_cmds = make_device_buf(cmd_bytes, cmd_usage);
    const VkDeviceSize count_bytes = sizeof(uint32_t) * k_max_mesh_batches;
    frame.early_counts = make_device_buf(count_bytes, count_usage);
    frame.late_counts = make_device_buf(count_bytes, count_usage);
    frame.shadow_counts = make_device_buf(count_bytes, count_usage);
    frame.dummy_counts = make_device_buf(count_bytes, count_usage);
    frame.early_count_readback = eastl::make_unique<VulkanBuffer>();
    frame.early_count_readback->create(m_allocator, count_bytes,
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       VMA_MEMORY_USAGE_GPU_TO_CPU);
    frame.late_count_readback = eastl::make_unique<VulkanBuffer>();
    frame.late_count_readback->create(m_allocator, count_bytes,
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VMA_MEMORY_USAGE_GPU_TO_CPU);
    frame.batch_bases =
        make_host_buf(count_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.unique_counts =
        make_host_buf(count_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.unique_firsts =
        make_host_buf(count_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const VkDeviceSize unique_bytes =
        sizeof(uint32_t) * k_max_gpu_driven_meshlets;
    const VkBufferUsageFlags id_usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    frame.compact_bases =
        make_host_buf(unique_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.unique_batch =
        make_host_buf(unique_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.unique_expanded =
        make_host_buf(unique_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    frame.early_instance_counts = make_device_buf(unique_bytes, id_usage);
    frame.late_instance_counts = make_device_buf(unique_bytes, id_usage);
    frame.shadow_instance_counts = make_device_buf(unique_bytes, id_usage);
    frame.dummy_instance_counts = make_device_buf(unique_bytes, id_usage);
    frame.early_compact_ids = make_device_buf(unique_bytes, id_usage);
    frame.late_compact_ids = make_device_buf(unique_bytes, id_usage);
    frame.shadow_compact_ids = make_device_buf(unique_bytes, id_usage);
    frame.dummy_compact_ids = make_device_buf(unique_bytes, id_usage);
    frame.cull_ubo = make_host_buf(sizeof(GpuDrivenCullUniforms),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.shadow_cull_ubo = make_host_buf(sizeof(GpuDrivenCullUniforms),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.emit_ubo = make_host_buf(sizeof(GpuDrivenEmitUniforms),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.shadow_emit_ubo = make_host_buf(sizeof(GpuDrivenEmitUniforms),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.view_ubo = make_host_buf(sizeof(ForwardMeshUniformData),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.gbuffer_ubo = make_host_buf(sizeof(GpuDrivenGBufferUniformData),
                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    frame.shadow_ubo = make_host_buf(sizeof(GpuDrivenShadowUniformData),
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    for (uint32_t mip = 0; mip < k_max_hiz_mips; ++mip) {
      frame.hiz_ubos[mip] =
          make_host_buf(sizeof(HizUniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }
  }
}

void GpuDrivenRenderer::destroyBuffers() {
  for (uint32_t i = 0; i < k_frames; ++i) {
    FrameBuffers& frame = m_frames[i];
    auto drop = [](eastl::unique_ptr<VulkanBuffer>& buf) {
      if (buf) {
        buf->destroy();
        buf.reset();
      }
    };
    drop(frame.instances);
    drop(frame.meshlets);
    drop(frame.early_cmds);
    drop(frame.late_cmds);
    drop(frame.shadow_cmds);
    drop(frame.dummy_cmds);
    drop(frame.early_counts);
    drop(frame.late_counts);
    drop(frame.shadow_counts);
    drop(frame.dummy_counts);
    drop(frame.early_count_readback);
    drop(frame.late_count_readback);
    drop(frame.batch_bases);
    drop(frame.unique_counts);
    drop(frame.unique_firsts);
    drop(frame.compact_bases);
    drop(frame.unique_batch);
    drop(frame.unique_expanded);
    drop(frame.early_instance_counts);
    drop(frame.late_instance_counts);
    drop(frame.shadow_instance_counts);
    drop(frame.dummy_instance_counts);
    drop(frame.early_compact_ids);
    drop(frame.late_compact_ids);
    drop(frame.shadow_compact_ids);
    drop(frame.dummy_compact_ids);
    drop(frame.cull_ubo);
    drop(frame.shadow_cull_ubo);
    drop(frame.emit_ubo);
    drop(frame.shadow_emit_ubo);
    drop(frame.view_ubo);
    drop(frame.gbuffer_ubo);
    drop(frame.shadow_ubo);
    for (uint32_t mip = 0; mip < k_max_hiz_mips; ++mip) {
      drop(frame.hiz_ubos[mip]);
    }
  }
}

void GpuDrivenRenderer::createComputePipeline(
    const char* shader_path, const uint32_t* bindings, const uint32_t* sets,
    uint32_t binding_count, const ShaderDescriptorKind* kinds,
    VkDescriptorSetLayout* layout, VkPipelineLayout* pipe_layout,
    VkPipeline* pipeline) {
  const SlangCompiler::ComputeProgramResult program =
      m_compiler->compileComputeProgram(shader_path, "main");
  if (!shaderResourceBindingsMatch(program.layout, bindings, binding_count, sets,
                                   kinds)) {
    LOG_FATAL(
        "[GpuDrivenRenderer] Shader resource layout does not match record-path "
        "bindings for '{}' (extracted {} bindings, expected {})",
        shader_path, program.layout.count, binding_count);
  }

  eastl::vector<VkDescriptorSetLayoutBinding> layout_bindings;
  for (uint32_t i = 0; i < program.layout.count; ++i) {
    const ShaderResourceBinding& resource = program.layout.bindings[i];
    if (resource.set != 0) {
      continue;
    }
    VkDescriptorSetLayoutBinding b{};
    b.binding = resource.binding;
    b.descriptorType = descriptorType(resource.kind);
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    layout_bindings.push_back(b);
  }
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
  layout_info.pBindings = layout_bindings.data();
  vkCreateDescriptorSetLayout(m_context->getDevice(), &layout_info, nullptr, layout);

  VkPipelineLayoutCreateInfo pipe_layout_info{};
  pipe_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipe_layout_info.setLayoutCount = 1;
  pipe_layout_info.pSetLayouts = layout;
  vkCreatePipelineLayout(m_context->getDevice(), &pipe_layout_info, nullptr,
                         pipe_layout);

  VkShaderModuleCreateInfo module_info{};
  module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_info.codeSize = program.compute.spirv_code.size();
  module_info.pCode =
      reinterpret_cast<const uint32_t*>(program.compute.spirv_code.data());
  VkShaderModule module = VK_NULL_HANDLE;
  vkCreateShaderModule(m_context->getDevice(), &module_info, nullptr, &module);
  *pipeline = createVkComputePipeline(m_context->getDevice(), *pipe_layout, module,
                                      program.compute.entry_point_name.c_str());
  vkDestroyShaderModule(m_context->getDevice(), module, nullptr);
}

void GpuDrivenRenderer::createPipelines(VkRenderPass offscreen_pass,
                                        VkRenderPass shadow_pass,
                                        VkRenderPass gbuffer_pass) {
  VulkanPipelineCreateInfo pbr{};
  pbr.shader_path = "engine/shaders/pbr_gpu_driven.slang";
  pbr.enable_vertex_input = true;
  pbr.cull_mode = VK_CULL_MODE_NONE;
  pbr.enable_depth_test = true;
  pbr.enable_depth_write = true;
  fillGpuDrivenPbrExpectedBindings(pbr.expected_descriptor_bindings,
                                   pbr.expected_descriptor_sets,
                                   &pbr.expected_descriptor_binding_count,
                                   pbr.expected_descriptor_kinds);
  m_pbr_pipeline = eastl::make_unique<VulkanPipeline>();
  m_pbr_pipeline->initialize(m_context, m_compiler, offscreen_pass, pbr);

  if (shadow_pass != VK_NULL_HANDLE) {
    VulkanPipelineCreateInfo shadow{};
    shadow.shader_path = "engine/shaders/shadow_gpu_driven.slang";
    shadow.enable_vertex_input = true;
    shadow.cull_mode = VK_CULL_MODE_BACK_BIT;
    shadow.enable_depth_test = true;
    shadow.enable_depth_write = true;
    shadow.depth_compare_op = VK_COMPARE_OP_LESS;
    shadow.depth_only_subpass = true;
    fillGpuDrivenShadowExpectedBindings(
        shadow.expected_descriptor_bindings, shadow.expected_descriptor_sets,
        &shadow.expected_descriptor_binding_count, shadow.expected_descriptor_kinds);
    m_shadow_pipeline = eastl::make_unique<VulkanPipeline>();
    m_shadow_pipeline->initialize(m_context, m_compiler, shadow_pass, shadow);
  }

  if (gbuffer_pass != VK_NULL_HANDLE) {
    VulkanPipelineCreateInfo gbuffer{};
    gbuffer.shader_path = "engine/shaders/gbuffer_gpu_driven.slang";
    gbuffer.enable_vertex_input = true;
    gbuffer.cull_mode = VK_CULL_MODE_NONE;
    gbuffer.enable_depth_test = true;
    gbuffer.enable_depth_write = true;
    gbuffer.color_attachment_count = 3;
    fillGpuDrivenGBufferExpectedBindings(
        gbuffer.expected_descriptor_bindings, gbuffer.expected_descriptor_sets,
        &gbuffer.expected_descriptor_binding_count,
        gbuffer.expected_descriptor_kinds);
    m_gbuffer_pipeline = eastl::make_unique<VulkanPipeline>();
    m_gbuffer_pipeline->initialize(m_context, m_compiler, gbuffer_pass, gbuffer);
  }

  uint32_t cull_bindings[k_max_expected_descriptor_bindings];
  uint32_t cull_sets[k_max_expected_descriptor_bindings];
  ShaderDescriptorKind cull_kinds[k_max_expected_descriptor_bindings];
  uint32_t cull_count = 0;
  fillMeshletCullExpectedBindings(cull_bindings, cull_sets, &cull_count, cull_kinds);
  createComputePipeline("engine/shaders/meshlet_cull.slang", cull_bindings, cull_sets,
                        cull_count, cull_kinds, &m_cull_layout, &m_cull_pipe_layout,
                        &m_cull_pipeline);

  uint32_t emit_bindings[k_max_expected_descriptor_bindings];
  uint32_t emit_sets[k_max_expected_descriptor_bindings];
  ShaderDescriptorKind emit_kinds[k_max_expected_descriptor_bindings];
  uint32_t emit_count = 0;
  fillMeshletEmitExpectedBindings(emit_bindings, emit_sets, &emit_count, emit_kinds);
  createComputePipeline("engine/shaders/meshlet_emit.slang", emit_bindings, emit_sets,
                        emit_count, emit_kinds, &m_emit_layout, &m_emit_pipe_layout,
                        &m_emit_pipeline);

  uint32_t hiz_bindings[k_max_expected_descriptor_bindings];
  uint32_t hiz_sets[k_max_expected_descriptor_bindings];
  ShaderDescriptorKind hiz_kinds[k_max_expected_descriptor_bindings];
  uint32_t hiz_count = 0;
  fillHizPyramidExpectedBindings(hiz_bindings, hiz_sets, &hiz_count, hiz_kinds);
  createComputePipeline("engine/shaders/hiz_pyramid.slang", hiz_bindings, hiz_sets,
                        hiz_count, hiz_kinds, &m_hiz_layout, &m_hiz_pipe_layout,
                        &m_hiz_pipeline);

  if (m_mesh_shaders_enabled) {
    createMeshPipeline(offscreen_pass);
  }
}

void GpuDrivenRenderer::createMeshPipeline(VkRenderPass render_pass) {
  const SlangCompiler::MeshProgramResult program =
      m_compiler->compileMeshProgram("engine/shaders/pbr_mesh.slang");
  uint32_t bindings[k_max_expected_descriptor_bindings];
  uint32_t sets[k_max_expected_descriptor_bindings];
  ShaderDescriptorKind kinds[k_max_expected_descriptor_bindings];
  uint32_t count = 0;
  fillGpuDrivenMeshExpectedBindings(bindings, sets, &count, kinds);
  if (!shaderResourceBindingsMatch(program.layout, bindings, count, sets, kinds)) {
    LOG_FATAL(
        "[GpuDrivenRenderer] Shader resource layout does not match record-path "
        "bindings for 'engine/shaders/pbr_mesh.slang' (extracted {} bindings, "
        "expected {})",
        program.layout.count, count);
  }

  eastl::vector<VkDescriptorSetLayoutBinding> layout_bindings;
  bool uses_bindless = false;
  for (uint32_t i = 0; i < program.layout.count; ++i) {
    const ShaderResourceBinding& resource = program.layout.bindings[i];
    if (resource.set == 1) {
      uses_bindless = true;
      continue;
    }
    VkDescriptorSetLayoutBinding b{};
    b.binding = resource.binding;
    b.descriptorType = descriptorType(resource.kind);
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                   VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_bindings.push_back(b);
  }
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
  layout_info.pBindings = layout_bindings.data();
  vkCreateDescriptorSetLayout(m_context->getDevice(), &layout_info, nullptr,
                              &m_mesh_layout);

  VkDescriptorSetLayout set_layouts[2] = {
      m_mesh_layout, m_context->bindlessTextureTable().descriptorSetLayout()};
  VkPipelineLayoutCreateInfo pipe_layout_info{};
  pipe_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipe_layout_info.setLayoutCount = uses_bindless ? 2u : 1u;
  pipe_layout_info.pSetLayouts = set_layouts;
  vkCreatePipelineLayout(m_context->getDevice(), &pipe_layout_info, nullptr,
                         &m_mesh_pipe_layout);

  auto make_module = [this](const eastl::vector<uint8_t>& spirv) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spirv.size();
    info.pCode = reinterpret_cast<const uint32_t*>(spirv.data());
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule(m_context->getDevice(), &info, nullptr, &module);
    return module;
  };
  VkShaderModule task_mod = make_module(program.task.spirv_code);
  VkShaderModule mesh_mod = make_module(program.mesh.spirv_code);
  VkShaderModule frag_mod = make_module(program.fragment.spirv_code);

  VkPipelineShaderStageCreateInfo stages[3]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
  stages[0].module = task_mod;
  stages[0].pName = program.task.entry_point_name.c_str();
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
  stages[1].module = mesh_mod;
  stages[1].pName = program.mesh.entry_point_name.c_str();
  stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[2].module = frag_mod;
  stages[2].pName = program.fragment.entry_point_name.c_str();

  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.lineWidth = 1.0f;
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  VkPipelineMultisampleStateCreateInfo ms{};
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineColorBlendAttachmentState blend_attach{};
  blend_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1;
  blend.pAttachments = &blend_attach;
  VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dyn{};
  dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dyn.dynamicStateCount = 2;
  dyn.pDynamicStates = dyn_states;
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth.depthTestEnable = VK_TRUE;
  depth.depthWriteEnable = VK_TRUE;
  depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

  VkGraphicsPipelineCreateInfo gp{};
  gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  gp.stageCount = 3;
  gp.pStages = stages;
  gp.pViewportState = &viewport;
  gp.pRasterizationState = &raster;
  gp.pMultisampleState = &ms;
  gp.pColorBlendState = &blend;
  gp.pDynamicState = &dyn;
  gp.pDepthStencilState = &depth;
  gp.layout = m_mesh_pipe_layout;
  gp.renderPass = render_pass;
  const VkResult result =
      m_context->createGraphicsPipelines(1, &gp, &m_mesh_pipeline);
  vkDestroyShaderModule(m_context->getDevice(), task_mod, nullptr);
  vkDestroyShaderModule(m_context->getDevice(), mesh_mod, nullptr);
  vkDestroyShaderModule(m_context->getDevice(), frag_mod, nullptr);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[GpuDrivenRenderer] mesh pipeline create failed: {}",
              static_cast<int>(result));
  }
}

void GpuDrivenRenderer::destroyPipelines() {
  VkDevice device = m_context != nullptr ? m_context->getDevice() : VK_NULL_HANDLE;
  auto drop_pipe = [&](VkPipeline& p) {
    if (device != VK_NULL_HANDLE && p != VK_NULL_HANDLE) {
      vkDestroyPipeline(device, p, nullptr);
      p = VK_NULL_HANDLE;
    }
  };
  auto drop_layout = [&](VkPipelineLayout& p) {
    if (device != VK_NULL_HANDLE && p != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(device, p, nullptr);
      p = VK_NULL_HANDLE;
    }
  };
  auto drop_set_layout = [&](VkDescriptorSetLayout& p) {
    if (device != VK_NULL_HANDLE && p != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device, p, nullptr);
      p = VK_NULL_HANDLE;
    }
  };
  drop_pipe(m_mesh_pipeline);
  drop_layout(m_mesh_pipe_layout);
  drop_set_layout(m_mesh_layout);
  drop_pipe(m_hiz_pipeline);
  drop_layout(m_hiz_pipe_layout);
  drop_set_layout(m_hiz_layout);
  drop_pipe(m_cull_pipeline);
  drop_layout(m_cull_pipe_layout);
  drop_set_layout(m_cull_layout);
  drop_pipe(m_emit_pipeline);
  drop_layout(m_emit_pipe_layout);
  drop_set_layout(m_emit_layout);
  if (m_gbuffer_pipeline) {
    m_gbuffer_pipeline->shutdown();
    m_gbuffer_pipeline.reset();
  }
  if (m_shadow_pipeline) {
    m_shadow_pipeline->shutdown();
    m_shadow_pipeline.reset();
  }
  if (m_pbr_pipeline) {
    m_pbr_pipeline->shutdown();
    m_pbr_pipeline.reset();
  }
}

void GpuDrivenRenderer::createDescriptors() {
  VkDevice device = m_context->getDevice();
  const uint32_t mesh_set_count = k_frames * 2u * k_max_mesh_batches;
  const uint32_t hiz_set_count = k_frames * k_max_hiz_mips;
  VkDescriptorPoolSize sizes[5]{};
  sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  sizes[0].descriptorCount = 64u + hiz_set_count + k_frames * 8u;
  sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  sizes[1].descriptorCount = 128u + mesh_set_count * 10u + k_frames * 80u;
  sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  sizes[2].descriptorCount = 64u + hiz_set_count + mesh_set_count * 3u + k_frames * 16u;
  sizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  sizes[3].descriptorCount = 64u + hiz_set_count + k_frames * 8u;
  sizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  sizes[4].descriptorCount = eastl::max(16u, hiz_set_count);
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.poolSizeCount = 5;
  pool_info.pPoolSizes = sizes;
  pool_info.maxSets = 32u + mesh_set_count + hiz_set_count + k_frames * 8u;
  if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool) !=
      VK_SUCCESS) {
    LOG_FATAL("[GpuDrivenRenderer] vkCreateDescriptorPool failed");
  }

  auto alloc = [&](VkDescriptorSetLayout layout, uint32_t count, VkDescriptorSet* out) {
    eastl::vector<VkDescriptorSetLayout> layouts(count, layout);
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = m_descriptor_pool;
    info.descriptorSetCount = count;
    info.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(device, &info, out) != VK_SUCCESS) {
      LOG_FATAL("[GpuDrivenRenderer] vkAllocateDescriptorSets failed");
    }
  };
  alloc(m_cull_layout, k_frames, m_cull_sets);
  alloc(m_cull_layout, k_frames, m_shadow_cull_sets);
  alloc(m_emit_layout, k_frames, m_emit_sets);
  alloc(m_emit_layout, k_frames, m_shadow_emit_sets);
  for (uint32_t f = 0; f < k_frames; ++f) {
    alloc(m_hiz_layout, k_max_hiz_mips, m_hiz_sets[f]);
  }
  alloc(m_pbr_pipeline->getDescriptorSetLayout(), k_frames, m_pbr_sets);
  if (m_gbuffer_pipeline) {
    alloc(m_gbuffer_pipeline->getDescriptorSetLayout(), k_frames, m_gbuffer_sets);
  }
  if (m_shadow_pipeline) {
    alloc(m_shadow_pipeline->getDescriptorSetLayout(), k_frames, m_shadow_sets);
  }
  if (m_mesh_shaders_enabled && m_mesh_layout != VK_NULL_HANDLE) {
    for (uint32_t f = 0; f < k_frames; ++f) {
      alloc(m_mesh_layout, k_max_mesh_batches, m_mesh_sets[f][0]);
      alloc(m_mesh_layout, k_max_mesh_batches, m_mesh_sets[f][1]);
    }
  }
}

void GpuDrivenRenderer::destroyDescriptors() {
  if (m_context != nullptr && m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_context->getDevice(), m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
  }
}

void GpuDrivenRenderer::createHiz(uint32_t width, uint32_t height) {
  destroyHiz();
  if (m_context == nullptr || m_allocator == nullptr) {
    return;
  }
  m_hiz_width = eastl::max(1u, width);
  m_hiz_height = eastl::max(1u, height);

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_NEAREST;
  sampler_info.minFilter = VK_FILTER_NEAREST;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkCreateSampler(m_context->getDevice(), &sampler_info, nullptr, &m_hiz_sampler);

  auto create_pyramid = [&](HizPyramid& pyramid, uint32_t w, uint32_t h) {
    pyramid.width = w;
    pyramid.height = h;
    pyramid.mip_count = 1;
    uint32_t mw = w;
    uint32_t mh = h;
    while (mw > 1 || mh > 1) {
      mw = eastl::max(1u, mw / 2u);
      mh = eastl::max(1u, mh / 2u);
      ++pyramid.mip_count;
      if (pyramid.mip_count >= k_max_hiz_mips) {
        break;
      }
    }
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R32_SFLOAT;
    image_info.extent = {w, h, 1};
    image_info.mipLevels = pyramid.mip_count;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaCreateImage(m_allocator->getAllocator(), &image_info, &alloc_info,
                   &pyramid.image, &pyramid.allocation, nullptr);
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = pyramid.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R32_SFLOAT;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = pyramid.mip_count;
    view_info.subresourceRange.layerCount = 1;
    vkCreateImageView(m_context->getDevice(), &view_info, nullptr,
                      &pyramid.sampled_view);
    for (uint32_t mip = 0; mip < pyramid.mip_count; ++mip) {
      view_info.subresourceRange.baseMipLevel = mip;
      view_info.subresourceRange.levelCount = 1;
      vkCreateImageView(m_context->getDevice(), &view_info, nullptr,
                        &pyramid.mip_views[mip]);
    }
    pyramid.built = false;
  };

  for (uint32_t i = 0; i < k_frames; ++i) {
    create_pyramid(m_hiz[i], m_hiz_width, m_hiz_height);
  }

  VkImageCreateInfo dummy_info{};
  dummy_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  dummy_info.imageType = VK_IMAGE_TYPE_2D;
  dummy_info.format = VK_FORMAT_R32_SFLOAT;
  dummy_info.extent = {1, 1, 1};
  dummy_info.mipLevels = 1;
  dummy_info.arrayLayers = 1;
  dummy_info.samples = VK_SAMPLE_COUNT_1_BIT;
  dummy_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  dummy_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  dummy_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VmaAllocationCreateInfo dummy_alloc{};
  dummy_alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  vmaCreateImage(m_allocator->getAllocator(), &dummy_info, &dummy_alloc,
                 &m_dummy_hiz_image, &m_dummy_hiz_alloc, nullptr);
  VkImageViewCreateInfo dummy_view{};
  dummy_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  dummy_view.image = m_dummy_hiz_image;
  dummy_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  dummy_view.format = VK_FORMAT_R32_SFLOAT;
  dummy_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  dummy_view.subresourceRange.levelCount = 1;
  dummy_view.subresourceRange.layerCount = 1;
  vkCreateImageView(m_context->getDevice(), &dummy_view, nullptr, &m_dummy_hiz_view);

  VkCommandBuffer cmd = m_context->beginImmediateCommands();
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.image = m_dummy_hiz_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
  m_context->endImmediateCommands(cmd);
}

void GpuDrivenRenderer::destroyHiz() {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  auto drop_pyramid = [&](HizPyramid& pyramid) {
    for (uint32_t i = 0; i < k_max_hiz_mips; ++i) {
      if (pyramid.mip_views[i] != VK_NULL_HANDLE) {
        vkDestroyImageView(device, pyramid.mip_views[i], nullptr);
        pyramid.mip_views[i] = VK_NULL_HANDLE;
      }
    }
    if (pyramid.sampled_view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, pyramid.sampled_view, nullptr);
      pyramid.sampled_view = VK_NULL_HANDLE;
    }
    if (pyramid.image != VK_NULL_HANDLE) {
      vmaDestroyImage(m_allocator->getAllocator(), pyramid.image, pyramid.allocation);
      pyramid.image = VK_NULL_HANDLE;
      pyramid.allocation = VK_NULL_HANDLE;
    }
    pyramid.built = false;
  };
  for (uint32_t i = 0; i < k_frames; ++i) {
    drop_pyramid(m_hiz[i]);
  }
  if (m_dummy_hiz_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, m_dummy_hiz_view, nullptr);
    m_dummy_hiz_view = VK_NULL_HANDLE;
  }
  if (m_dummy_hiz_image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), m_dummy_hiz_image, m_dummy_hiz_alloc);
    m_dummy_hiz_image = VK_NULL_HANDLE;
    m_dummy_hiz_alloc = VK_NULL_HANDLE;
  }
  if (m_hiz_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_hiz_sampler, nullptr);
    m_hiz_sampler = VK_NULL_HANDLE;
  }
  std::memset(m_hiz_bound_src, 0, sizeof(m_hiz_bound_src));
  std::memset(m_hiz_bound_dst, 0, sizeof(m_hiz_bound_dst));
  std::memset(m_cull_hiz_bound, 0, sizeof(m_cull_hiz_bound));
  std::memset(m_shadow_cull_hiz_bound, 0, sizeof(m_shadow_cull_hiz_bound));
}

void GpuDrivenRenderer::resizeHiZ(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return;
  }
  if (width == m_hiz_width && height == m_hiz_height && m_hiz[0].image != VK_NULL_HANDLE) {
    return;
  }
  createHiz(width, height);
}

void GpuDrivenRenderer::invalidateSceneOcclusion() {
  for (uint32_t i = 0; i < k_frames; ++i) {
    m_hiz[i].built = false;
    m_uploaded_fingerprint[i] = 0;
    m_uploaded_identity_fingerprint[i] = 0;
  }
  m_identity_fingerprint = 0;
  m_transform_fingerprint = 0;
  m_packed_fingerprint = 0;
  m_mask_fingerprint = 0;
  m_lights_fingerprint = 0;
  std::memset(m_cull_hiz_bound, 0, sizeof(m_cull_hiz_bound));
  std::memset(m_shadow_cull_hiz_bound, 0, sizeof(m_shadow_cull_hiz_bound));
}

void GpuDrivenRenderer::packDraws(const GpuDrivenDraw* draws, uint32_t count,
                                  const ForwardFrameState& frame_state) {
  m_packed_draws.clear();
  m_batches.clear();
  m_instance_cpu.clear();
  m_meshlet_cpu.clear();
  m_batch_base_cpu.clear();
  m_unique_count_cpu.clear();
  m_unique_first_cpu.clear();
  m_compact_base_cpu.clear();
  m_unique_batch_cpu.clear();
  m_unique_expanded_cpu.clear();
  m_instance_count = 0;
  m_meshlet_count = 0;
  m_unique_meshlet_count = 0;
  if (draws == nullptr || count == 0) {
    return;
  }
  BindlessTextureTable* table = &m_context->bindlessTextureTable();
  VulkanTexture* fallback = table->fallback();
  GpuMesh* current_mesh = nullptr;
  for (uint32_t i = 0; i < count; ++i) {
    const GpuDrivenDraw& draw = draws[i];
    if (draw.gpu_mesh == nullptr || !draw.gpu_mesh->hasMeshlets()) {
      continue;
    }
    if (m_instance_count >= k_max_gpu_driven_instances) {
      break;
    }
    const eastl::vector<GpuMeshletGpuRecord>& records =
        draw.gpu_mesh->getMeshletRecords();
    if (m_meshlet_count + records.size() > k_max_gpu_driven_meshlets) {
      break;
    }
    const bool new_batch =
        current_mesh != draw.gpu_mesh || m_batches.empty();
    if (new_batch && m_batches.size() >= k_max_mesh_batches) {
      LOG_ERROR(
          "[GpuDrivenRenderer] mesh batch limit reached ({}); remaining "
          "GPU-driven draws are dropped",
          k_max_mesh_batches);
      break;
    }
    if (new_batch &&
        m_unique_meshlet_count + records.size() > k_max_gpu_driven_meshlets) {
      break;
    }
    GpuDrivenDraw packed = draw;
    packed.receiver_id = k_gpu_driven_cpu_receiver_slots + m_instance_count;
    m_packed_draws.push_back(packed);

    GpuDrivenInstanceGpu inst{};
    fillInstance(inst, packed, table, fallback, frame_state);
    inst.meshlet_offset = m_meshlet_count;
    inst.meshlet_count = static_cast<uint32_t>(records.size());
    m_instance_cpu.push_back(inst);

    if (new_batch) {
      MeshBatch batch{};
      batch.mesh = draw.gpu_mesh;
      batch.expanded_first = m_meshlet_count;
      batch.meshlet_first = m_unique_meshlet_count;
      batch.meshlet_count = static_cast<uint32_t>(records.size());
      batch.instance_first = m_instance_count;
      batch.instance_count = 0;
      m_batches.push_back(batch);
      current_mesh = draw.gpu_mesh;
    }
    m_batches.back().instance_count += 1;
    const uint32_t batch_index = static_cast<uint32_t>(m_batches.size() - 1u);
    const uint32_t skip_cone_bit =
        draw.double_sided ? k_gpu_driven_flag_skip_cone : 0u;
    for (const GpuMeshletGpuRecord& rec : records) {
      GpuDrivenMeshletGpu meshlet{};
      meshlet.sphere = rec.sphere;
      meshlet.cone = rec.cone;
      meshlet.instance_index = m_instance_count;
      meshlet.first_index = rec.first_index;
      meshlet.index_count = rec.index_count;
      meshlet.flags =
          (batch_index << k_gpu_driven_meshlet_batch_shift) | skip_cone_bit;
      meshlet.vertex_offset = rec.vertex_offset;
      meshlet.vertex_count = rec.vertex_count;
      meshlet.triangle_offset = rec.triangle_offset;
      meshlet.triangle_count = rec.triangle_count;
      m_meshlet_cpu.push_back(meshlet);
      ++m_meshlet_count;
    }
    ++m_instance_count;
  }

  m_batch_base_cpu.resize(m_batches.size());
  m_unique_count_cpu.resize(m_batches.size());
  m_unique_first_cpu.resize(m_batches.size());
  m_unique_meshlet_count = 0;
  uint32_t compact_cursor = 0;
  for (uint32_t b = 0; b < m_batches.size(); ++b) {
    MeshBatch& batch = m_batches[b];
    m_batch_base_cpu[b] = batch.expanded_first;
    m_unique_first_cpu[b] = m_unique_meshlet_count;
    m_unique_count_cpu[b] = batch.meshlet_count;
    batch.meshlet_first = m_unique_meshlet_count;
    for (uint32_t m = 0; m < batch.meshlet_count; ++m) {
      m_compact_base_cpu.push_back(compact_cursor);
      m_unique_batch_cpu.push_back(b);
      m_unique_expanded_cpu.push_back(batch.expanded_first + m);
      compact_cursor += batch.instance_count;
      ++m_unique_meshlet_count;
    }
  }
  ASSERT(compact_cursor <= k_max_gpu_driven_meshlets);
  LOG_INFO(
      "[GpuDrivenRenderer] packed {} instances, {} meshlets, {} unique, {} "
      "batches; compact_indirect={}",
      m_instance_count, m_meshlet_count, m_unique_meshlet_count,
      static_cast<uint32_t>(m_batches.size()),
      compactIndirectEnabled() ? 1 : 0);
}

void GpuDrivenRenderer::updateInstanceTransforms(const GpuDrivenDraw* draws,
                                                 uint32_t count) {
  if (draws == nullptr) {
    return;
  }
  const uint32_t n = eastl::min(m_instance_count, count);
  for (uint32_t i = 0; i < n; ++i) {
    m_packed_draws[i].model = draws[i].model;
    m_instance_cpu[i].world = draws[i].model;
    m_instance_cpu[i].normal_matrix =
        glm::inverseTranspose(glm::mat4(glm::mat3(draws[i].model)));
  }
}

void GpuDrivenRenderer::updateCullDescriptors(uint32_t frame, VkDescriptorSet set,
                                              VulkanBuffer* ubo, VkImageView hiz_view,
                                              bool hiz_enabled, bool shadow) {
  ASSERT(set != VK_NULL_HANDLE);
  ASSERT(ubo != nullptr);
  FrameBuffers& buffers = m_frames[frame];
  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = ubo->getBuffer();
  ubo_info.range = sizeof(GpuDrivenCullUniforms);
  VkDescriptorBufferInfo instances{};
  instances.buffer = buffers.instances->getBuffer();
  instances.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo meshlets{};
  meshlets.buffer = buffers.meshlets->getBuffer();
  meshlets.range = VK_WHOLE_SIZE;
  VkDescriptorImageInfo hiz{};
  hiz.imageView = hiz_enabled ? hiz_view : m_dummy_hiz_view;
  hiz.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  hiz.sampler = m_hiz_sampler;
  auto ssbo = [](VulkanBuffer* buf) {
    VkDescriptorBufferInfo info{};
    info.buffer = buf->getBuffer();
    info.range = VK_WHOLE_SIZE;
    return info;
  };
  VkDescriptorBufferInfo batch_bases = ssbo(buffers.batch_bases.get());
  VkDescriptorBufferInfo unique_counts = ssbo(buffers.unique_counts.get());
  VkDescriptorBufferInfo unique_firsts = ssbo(buffers.unique_firsts.get());
  VkDescriptorBufferInfo compact_bases = ssbo(buffers.compact_bases.get());
  VkDescriptorBufferInfo early_inst =
      ssbo(shadow ? buffers.shadow_instance_counts.get()
                  : buffers.early_instance_counts.get());
  VkDescriptorBufferInfo late_inst =
      ssbo(shadow ? buffers.dummy_instance_counts.get()
                  : buffers.late_instance_counts.get());
  VkDescriptorBufferInfo early_ids =
      ssbo(shadow ? buffers.shadow_compact_ids.get()
                  : buffers.early_compact_ids.get());
  VkDescriptorBufferInfo late_ids =
      ssbo(shadow ? buffers.dummy_compact_ids.get()
                  : buffers.late_compact_ids.get());
  VkWriteDescriptorSet writes[13]{};
  for (uint32_t i = 0; i < 13; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = set;
    writes[i].descriptorCount = 1;
    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  }
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &ubo_info;
  writes[1].dstBinding = 1;
  writes[1].pBufferInfo = &instances;
  writes[2].dstBinding = 2;
  writes[2].pBufferInfo = &meshlets;
  writes[3].dstBinding = 3;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[3].pImageInfo = &hiz;
  writes[4].dstBinding = 4;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[4].pImageInfo = &hiz;
  writes[5].dstBinding = 5;
  writes[5].pBufferInfo = &batch_bases;
  writes[6].dstBinding = 6;
  writes[6].pBufferInfo = &unique_counts;
  writes[7].dstBinding = 7;
  writes[7].pBufferInfo = &unique_firsts;
  writes[8].dstBinding = 8;
  writes[8].pBufferInfo = &compact_bases;
  writes[9].dstBinding = 9;
  writes[9].pBufferInfo = &early_inst;
  writes[10].dstBinding = 10;
  writes[10].pBufferInfo = &late_inst;
  writes[11].dstBinding = 11;
  writes[11].pBufferInfo = &early_ids;
  writes[12].dstBinding = 12;
  writes[12].pBufferInfo = &late_ids;
  vkUpdateDescriptorSets(m_context->getDevice(), 13, writes, 0, nullptr);
}

bool GpuDrivenRenderer::compactIndirectEnabled() const {
  return m_context != nullptr && m_context->drawIndirectCountEnabled();
}

bool GpuDrivenRenderer::latePassEnabled() const {
  return m_late_pass_enabled;
}

void GpuDrivenRenderer::copyHudCounts(VkCommandBuffer cmd, uint32_t frame) {
  FrameBuffers& buffers = m_frames[frame];
  const uint32_t batch_count = static_cast<uint32_t>(m_batches.size());
  if (batch_count == 0 || buffers.early_count_readback == nullptr ||
      buffers.late_count_readback == nullptr) {
    m_hud_copied_batches[frame] = 0;
    return;
  }
  const VkDeviceSize bytes =
      static_cast<VkDeviceSize>(sizeof(uint32_t) * batch_count);
  VkBufferCopy copy{};
  copy.size = bytes;
  vkCmdCopyBuffer(cmd, buffers.early_counts->getBuffer(),
                  buffers.early_count_readback->getBuffer(), 1, &copy);
  vkCmdCopyBuffer(cmd, buffers.late_counts->getBuffer(),
                  buffers.late_count_readback->getBuffer(), 1, &copy);
  cmdBufferBarrier(cmd, buffers.early_count_readback->getBuffer(),
                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
  cmdBufferBarrier(cmd, buffers.late_count_readback->getBuffer(),
                   VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
  m_hud_copied_batches[frame] = batch_count;
}

void GpuDrivenRenderer::harvestHudCounts(uint32_t frame) {
  frame %= k_frames;
  FrameBuffers& buffers = m_frames[frame];
  const uint32_t n = m_hud_copied_batches[frame];
  if (n == 0 || n > k_max_mesh_batches ||
      buffers.early_count_readback == nullptr ||
      buffers.late_count_readback == nullptr) {
    return;
  }
  uint32_t early[k_max_mesh_batches];
  uint32_t late[k_max_mesh_batches];
  const VkDeviceSize bytes = static_cast<VkDeviceSize>(sizeof(uint32_t) * n);
  if (!buffers.early_count_readback->download(early, bytes) ||
      !buffers.late_count_readback->download(late, bytes)) {
    return;
  }
  m_surviving_early = sumCompactCountBuffer(early, n);
  m_surviving_late = sumCompactCountBuffer(late, n);
}

void GpuDrivenRenderer::updateEmitDescriptors(uint32_t frame, bool shadow) {
  FrameBuffers& buffers = m_frames[frame];
  VkDescriptorSet set = shadow ? m_shadow_emit_sets[frame] : m_emit_sets[frame];
  VulkanBuffer* ubo =
      shadow ? buffers.shadow_emit_ubo.get() : buffers.emit_ubo.get();
  ASSERT(set != VK_NULL_HANDLE);
  ASSERT(ubo != nullptr);
  auto ssbo = [](VulkanBuffer* buf) {
    VkDescriptorBufferInfo info{};
    info.buffer = buf->getBuffer();
    info.range = VK_WHOLE_SIZE;
    return info;
  };
  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = ubo->getBuffer();
  ubo_info.range = sizeof(GpuDrivenEmitUniforms);
  VkDescriptorBufferInfo meshlets = ssbo(buffers.meshlets.get());
  VkDescriptorBufferInfo unique_expanded = ssbo(buffers.unique_expanded.get());
  VkDescriptorBufferInfo unique_batch = ssbo(buffers.unique_batch.get());
  VkDescriptorBufferInfo batch_bases = ssbo(buffers.batch_bases.get());
  VkDescriptorBufferInfo unique_firsts = ssbo(buffers.unique_firsts.get());
  VkDescriptorBufferInfo compact_bases = ssbo(buffers.compact_bases.get());
  VkDescriptorBufferInfo early_inst =
      ssbo(shadow ? buffers.shadow_instance_counts.get()
                  : buffers.early_instance_counts.get());
  VkDescriptorBufferInfo late_inst =
      ssbo(shadow ? buffers.dummy_instance_counts.get()
                  : buffers.late_instance_counts.get());
  VkDescriptorBufferInfo early_cmds =
      ssbo(shadow ? buffers.shadow_cmds.get() : buffers.early_cmds.get());
  VkDescriptorBufferInfo late_cmds =
      ssbo(shadow ? buffers.dummy_cmds.get() : buffers.late_cmds.get());
  VkDescriptorBufferInfo early_counts =
      ssbo(shadow ? buffers.shadow_counts.get() : buffers.early_counts.get());
  VkDescriptorBufferInfo late_counts =
      ssbo(shadow ? buffers.dummy_counts.get() : buffers.late_counts.get());
  VkWriteDescriptorSet writes[13]{};
  for (uint32_t i = 0; i < 13; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = set;
    writes[i].descriptorCount = 1;
    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  }
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &ubo_info;
  writes[1].dstBinding = 1;
  writes[1].pBufferInfo = &meshlets;
  writes[2].dstBinding = 2;
  writes[2].pBufferInfo = &unique_expanded;
  writes[3].dstBinding = 3;
  writes[3].pBufferInfo = &unique_batch;
  writes[4].dstBinding = 4;
  writes[4].pBufferInfo = &batch_bases;
  writes[5].dstBinding = 5;
  writes[5].pBufferInfo = &unique_firsts;
  writes[6].dstBinding = 6;
  writes[6].pBufferInfo = &compact_bases;
  writes[7].dstBinding = 7;
  writes[7].pBufferInfo = &early_inst;
  writes[8].dstBinding = 8;
  writes[8].pBufferInfo = &late_inst;
  writes[9].dstBinding = 9;
  writes[9].pBufferInfo = &early_cmds;
  writes[10].dstBinding = 10;
  writes[10].pBufferInfo = &late_cmds;
  writes[11].dstBinding = 11;
  writes[11].pBufferInfo = &early_counts;
  writes[12].dstBinding = 12;
  writes[12].pBufferInfo = &late_counts;
  vkUpdateDescriptorSets(m_context->getDevice(), 13, writes, 0, nullptr);
}

void GpuDrivenRenderer::dispatchCull(VkCommandBuffer cmd, uint32_t frame,
                                     VkDescriptorSet set, VulkanBuffer* ubo,
                                     const glm::mat4& view_projection,
                                     const glm::vec3& camera, bool hiz_enabled,
                                     bool skip_cone, bool shadow) {
  if (m_meshlet_count == 0 || m_unique_meshlet_count == 0 ||
      m_cull_pipeline == VK_NULL_HANDLE || set == VK_NULL_HANDLE ||
      ubo == nullptr) {
    return;
  }
  FrameBuffers& buffers = m_frames[frame];
  VkBuffer early_inst = shadow ? buffers.shadow_instance_counts->getBuffer()
                               : buffers.early_instance_counts->getBuffer();
  VkBuffer late_inst = shadow ? buffers.dummy_instance_counts->getBuffer()
                              : buffers.late_instance_counts->getBuffer();
  VkBuffer early_ids = shadow ? buffers.shadow_compact_ids->getBuffer()
                              : buffers.early_compact_ids->getBuffer();
  VkBuffer late_ids = shadow ? buffers.dummy_compact_ids->getBuffer()
                             : buffers.late_compact_ids->getBuffer();
  const VkAccessFlags fill_src_access =
      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
  const VkPipelineStageFlags fill_src_stage =
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
      VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT |
      VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
  cmdBufferBarrier(cmd, early_inst, fill_src_access, VK_ACCESS_TRANSFER_WRITE_BIT,
                   fill_src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT);
  cmdBufferBarrier(cmd, late_inst, fill_src_access, VK_ACCESS_TRANSFER_WRITE_BIT,
                   fill_src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT);
  cmdBufferBarrier(cmd, early_ids, fill_src_access, VK_ACCESS_SHADER_WRITE_BIT,
                   fill_src_stage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, late_ids, fill_src_access, VK_ACCESS_SHADER_WRITE_BIT,
                   fill_src_stage, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  const uint32_t prev = (frame + k_frames - 1u) % k_frames;
  const bool can_hiz = hiz_enabled && m_hiz[prev].built;
  if (!shadow) {
    m_late_pass_enabled = can_hiz;
  }
  updateCullDescriptors(frame, set, ubo, m_hiz[prev].sampled_view, can_hiz,
                        shadow);

  GpuDrivenCullUniforms uniforms{};
  uniforms.view_projection = view_projection;
  uniforms.camera_position = glm::vec4(camera, 1.0f);
  extractFrustumPlanes(view_projection, uniforms.frustum_planes);
  uniforms.meshlet_count = m_meshlet_count;
  uniforms.hiz_enabled = can_hiz ? 1u : 0u;
  uniforms.hiz_mip_count = m_hiz[prev].mip_count;
  uniforms.skip_cone = skip_cone ? 1u : 0u;
  uniforms.compact_commands = compactIndirectEnabled() ? 1u : 0u;
  uniforms.hiz_size = glm::vec4(static_cast<float>(m_hiz[prev].width),
                                static_cast<float>(m_hiz[prev].height), 0.0f, 0.0f);
  ubo->upload(&uniforms, sizeof(uniforms));

  const VkDeviceSize inst_bytes =
      static_cast<VkDeviceSize>(sizeof(uint32_t) * m_unique_meshlet_count);
  vkCmdFillBuffer(cmd, early_inst, 0, inst_bytes, 0);
  vkCmdFillBuffer(cmd, late_inst, 0, inst_bytes, 0);
  cmdBufferBarrier(cmd, early_inst, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, late_inst, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  const VkPipelineStageFlags graphics_shader_stages =
      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
      VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT |
      VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
  cmdBufferBarrier(cmd, buffers.instances->getBuffer(), VK_ACCESS_HOST_WRITE_BIT,
                   VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | graphics_shader_stages);
  cmdBufferBarrier(cmd, buffers.meshlets->getBuffer(), VK_ACCESS_HOST_WRITE_BIT,
                   VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                       VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT |
                       VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT);
  cmdBufferBarrier(cmd, buffers.batch_bases->getBuffer(),
                   VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, buffers.unique_counts->getBuffer(),
                   VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, buffers.unique_firsts->getBuffer(),
                   VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, buffers.compact_bases->getBuffer(),
                   VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, ubo->getBuffer(), VK_ACCESS_HOST_WRITE_BIT,
                   VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cull_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_cull_pipe_layout, 0,
                          1, &set, 0, nullptr);
  const uint32_t groups = (m_meshlet_count + 63u) / 64u;
  vkCmdDispatch(cmd, groups, 1, 1);
  const VkAccessFlags compact_dst_access = VK_ACCESS_SHADER_READ_BIT;
  const VkPipelineStageFlags compact_dst_stage =
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | graphics_shader_stages;
  cmdBufferBarrier(cmd, early_inst, VK_ACCESS_SHADER_WRITE_BIT, compact_dst_access,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, compact_dst_stage);
  cmdBufferBarrier(cmd, late_inst, VK_ACCESS_SHADER_WRITE_BIT, compact_dst_access,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, compact_dst_stage);
  cmdBufferBarrier(cmd, early_ids, VK_ACCESS_SHADER_WRITE_BIT, compact_dst_access,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, compact_dst_stage);
  cmdBufferBarrier(cmd, late_ids, VK_ACCESS_SHADER_WRITE_BIT, compact_dst_access,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, compact_dst_stage);
  cmdBufferBarrier(cmd, buffers.instances->getBuffer(), VK_ACCESS_SHADER_READ_BIT,
                   VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   graphics_shader_stages);
}

void GpuDrivenRenderer::dispatchEmit(VkCommandBuffer cmd, uint32_t frame,
                                     bool shadow) {
  if (m_unique_meshlet_count == 0 || m_emit_pipeline == VK_NULL_HANDLE) {
    return;
  }
  FrameBuffers& buffers = m_frames[frame];
  VkDescriptorSet set = shadow ? m_shadow_emit_sets[frame] : m_emit_sets[frame];
  VulkanBuffer* ubo =
      shadow ? buffers.shadow_emit_ubo.get() : buffers.emit_ubo.get();
  if (set == VK_NULL_HANDLE || ubo == nullptr) {
    return;
  }
  VkBuffer early_cmds = shadow ? buffers.shadow_cmds->getBuffer()
                               : buffers.early_cmds->getBuffer();
  VkBuffer late_cmds = shadow ? buffers.dummy_cmds->getBuffer()
                              : buffers.late_cmds->getBuffer();
  VkBuffer early_counts = shadow ? buffers.shadow_counts->getBuffer()
                                 : buffers.early_counts->getBuffer();
  VkBuffer late_counts = shadow ? buffers.dummy_counts->getBuffer()
                                : buffers.late_counts->getBuffer();
  // Same PRIMARY may already have Viewport G-buffer INDIRECT_COMMAND_READ and
  // a HUD vkCmdCopyBuffer TRANSFER_READ on these buffers (Camera Preview recull).
  const VkAccessFlags fill_src_access = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
                                        VK_ACCESS_TRANSFER_READ_BIT |
                                        VK_ACCESS_SHADER_READ_BIT |
                                        VK_ACCESS_SHADER_WRITE_BIT;
  const VkPipelineStageFlags fill_src_stage =
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT |
      VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT |
      VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT |
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  cmdBufferBarrier(cmd, early_cmds, fill_src_access, VK_ACCESS_TRANSFER_WRITE_BIT,
                   fill_src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT);
  cmdBufferBarrier(cmd, late_cmds, fill_src_access, VK_ACCESS_TRANSFER_WRITE_BIT,
                   fill_src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT);
  cmdBufferBarrier(cmd, early_counts, fill_src_access, VK_ACCESS_TRANSFER_WRITE_BIT,
                   fill_src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT);
  cmdBufferBarrier(cmd, late_counts, fill_src_access, VK_ACCESS_TRANSFER_WRITE_BIT,
                   fill_src_stage, VK_PIPELINE_STAGE_TRANSFER_BIT);

  updateEmitDescriptors(frame, shadow);

  GpuDrivenEmitUniforms uniforms{};
  uniforms.unique_count = m_unique_meshlet_count;
  uniforms.compact_commands = compactIndirectEnabled() ? 1u : 0u;
  ubo->upload(&uniforms, sizeof(uniforms));

  const uint32_t batch_count =
      eastl::max(1u, static_cast<uint32_t>(m_batches.size()));
  const VkDeviceSize count_bytes =
      static_cast<VkDeviceSize>(sizeof(uint32_t) * batch_count);
  vkCmdFillBuffer(cmd, early_counts, 0, count_bytes, 0);
  vkCmdFillBuffer(cmd, late_counts, 0, count_bytes, 0);
  cmdBufferBarrier(cmd, early_counts, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, late_counts, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  const VkDeviceSize cmd_bytes = static_cast<VkDeviceSize>(
      sizeof(DrawIndexedIndirectCommand) * m_unique_meshlet_count);
  vkCmdFillBuffer(cmd, early_cmds, 0, cmd_bytes, 0);
  vkCmdFillBuffer(cmd, late_cmds, 0, cmd_bytes, 0);
  cmdBufferBarrier(cmd, early_cmds, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, late_cmds, VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, buffers.unique_expanded->getBuffer(),
                   VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, buffers.unique_batch->getBuffer(),
                   VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  cmdBufferBarrier(cmd, ubo->getBuffer(), VK_ACCESS_HOST_WRITE_BIT,
                   VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_emit_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_emit_pipe_layout, 0,
                          1, &set, 0, nullptr);
  const uint32_t groups = (m_unique_meshlet_count + 63u) / 64u;
  vkCmdDispatch(cmd, groups, 1, 1);
  const VkAccessFlags cmd_dst_access =
      VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
  const VkPipelineStageFlags cmd_dst_stage =
      VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT |
      VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
  cmdBufferBarrier(cmd, early_cmds, VK_ACCESS_SHADER_WRITE_BIT, cmd_dst_access,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_dst_stage);
  cmdBufferBarrier(cmd, late_cmds, VK_ACCESS_SHADER_WRITE_BIT, cmd_dst_access,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, cmd_dst_stage);
  cmdBufferBarrier(cmd, early_counts, VK_ACCESS_SHADER_WRITE_BIT,
                   VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
  cmdBufferBarrier(cmd, late_counts, VK_ACCESS_SHADER_WRITE_BIT,
                   VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                       VK_PIPELINE_STAGE_TRANSFER_BIT);
}

void GpuDrivenRenderer::uploadAndCull(VkCommandBuffer cmd, uint32_t frame,
                                      const GpuDrivenDraw* draws, uint32_t count,
                                      const ForwardFrameState& frame_state,
                                      bool enable_hiz, bool copy_hud_counts) {
  frame %= k_frames;
  eastl::vector<GpuDrivenDraw> sorted;
  const GpuDrivenDraw* pack_draws = draws;
  uint32_t pack_count = count;
  if (draws != nullptr && count > 1) {
    // One batch per unique GpuMesh. Entity order switches meshes every
    // instance (SE-world ~10k) and hits k_max_mesh_batches=512 after ~575
    // draws, dropping the forest and overflowing some record paths.
    sorted.assign(draws, draws + count);
    eastl::sort(sorted.begin(), sorted.end(),
                [](const GpuDrivenDraw& a, const GpuDrivenDraw& b) {
                  if (a.gpu_mesh != b.gpu_mesh) {
                    return a.gpu_mesh < b.gpu_mesh;
                  }
                  return a.entity_id < b.entity_id;
                });
    pack_draws = sorted.data();
    pack_count = static_cast<uint32_t>(sorted.size());
  }
  uint64_t identity = m_identity_fingerprint;
  uint64_t transforms = m_transform_fingerprint;
  if (!frame_state.scene_static || identity == 0) {
    identity = hashDrawIdentity(pack_draws, pack_count);
    transforms = hashDrawTransforms(pack_draws, pack_count);
  }
  bool packed_identity_changed = false;
  if (identity != m_identity_fingerprint) {
    packDraws(pack_draws, pack_count, frame_state);
    m_identity_fingerprint = identity;
    m_transform_fingerprint = transforms;
    packed_identity_changed = true;
  } else if (transforms != m_transform_fingerprint) {
    updateInstanceTransforms(pack_draws, pack_count);
    m_transform_fingerprint = transforms;
  }
  if (m_instance_count == 0) {
    m_late_pass_enabled = false;
    m_surviving_early = 0;
    m_surviving_late = 0;
    m_hud_copied_batches[frame] = 0;
    return;
  }

  EvaluatedLight scene_lights[k_max_forward_scene_lights];
  uint32_t scene_light_count = 0;
  ForwardMeshUniformData view_ubo{};
  view_ubo.view = frame_state.view;
  view_ubo.projection = frame_state.projection;
  view_ubo.camera_position = glm::vec4(frame_state.camera_position, 1.0f);
  applyPbrToMeshUniforms(view_ubo, nullptr, frame_state.shading, frame_state,
                         cgltf_alpha_mode_opaque, 0.5f, false,
                         k_invalid_entity_id);
  if (frame_state.live_scene_lighting && frame_state.lighting_scene != nullptr) {
    scene_light_count = static_cast<uint32_t>(buildDeferredLightList(
        *frame_state.lighting_scene, scene_lights, k_max_forward_scene_lights));
    view_ubo.light_count =
        glm::vec4(static_cast<float>(scene_light_count), 0.0f, 0.0f, 0.0f);
    for (uint32_t i = 0; i < scene_light_count; ++i) {
      packEvaluatedLight(view_ubo.lights[i], scene_lights[i],
                         frame_state.shadow_caster_id, frame_state.shadows_enabled,
                         &frame_state.local_shadows);
    }
  }
  if (frame_state.live_scene_lighting) {
    view_ubo.ambient_color =
        glm::max(view_ubo.ambient_color,
                 glm::vec4(k_live_lighting_ambient_floor));
  }

  const uint64_t lights = hashEvaluatedLights(scene_lights, scene_light_count);
  if (packed_identity_changed || lights != m_lights_fingerprint) {
    uint64_t mask_hash = 14695981039346656037ull;
    for (uint32_t i = 0; i < m_instance_count; ++i) {
      const uint32_t mask = gpuDrivenLightMask(
          frame_state, m_packed_draws[i].entity_id, scene_lights, scene_light_count);
      m_instance_cpu[i].light_mask = mask;
      mask_hash = hashMix(mask_hash, mask);
    }
    m_mask_fingerprint = mask_hash;
    m_lights_fingerprint = lights;
  }
  m_packed_fingerprint = identity ^ transforms ^ m_mask_fingerprint;

  FrameBuffers& buffers = m_frames[frame];
  if (m_uploaded_fingerprint[frame] != m_packed_fingerprint) {
    buffers.instances->upload(m_instance_cpu.data(),
                              sizeof(GpuDrivenInstanceGpu) * m_instance_count);
    m_uploaded_fingerprint[frame] = m_packed_fingerprint;
  }
  if (m_uploaded_identity_fingerprint[frame] != m_identity_fingerprint) {
    if (m_meshlet_count > 0) {
      buffers.meshlets->upload(m_meshlet_cpu.data(),
                               sizeof(GpuDrivenMeshletGpu) * m_meshlet_count);
    }
    if (!m_batch_base_cpu.empty()) {
      buffers.batch_bases->upload(m_batch_base_cpu.data(),
                                  sizeof(uint32_t) * m_batch_base_cpu.size());
    }
    if (!m_unique_count_cpu.empty()) {
      buffers.unique_counts->upload(m_unique_count_cpu.data(),
                                    sizeof(uint32_t) * m_unique_count_cpu.size());
      buffers.unique_firsts->upload(m_unique_first_cpu.data(),
                                    sizeof(uint32_t) * m_unique_first_cpu.size());
    }
    if (!m_compact_base_cpu.empty()) {
      buffers.compact_bases->upload(
          m_compact_base_cpu.data(),
          sizeof(uint32_t) * m_compact_base_cpu.size());
      buffers.unique_batch->upload(m_unique_batch_cpu.data(),
                                   sizeof(uint32_t) * m_unique_batch_cpu.size());
      buffers.unique_expanded->upload(
          m_unique_expanded_cpu.data(),
          sizeof(uint32_t) * m_unique_expanded_cpu.size());
    }
    m_uploaded_identity_fingerprint[frame] = m_identity_fingerprint;
  }

  buffers.view_ubo->upload(&view_ubo, sizeof(view_ubo));

  GpuDrivenGBufferUniformData gbuffer_ubo{};
  gbuffer_ubo.view = frame_state.view;
  gbuffer_ubo.projection = frame_state.projection;
  buffers.gbuffer_ubo->upload(&gbuffer_ubo, sizeof(gbuffer_ubo));

  GpuDrivenShadowUniformData shadow_ubo{};
  shadow_ubo.light_view = frame_state.light_view;
  shadow_ubo.light_projection = frame_state.light_projection;
  buffers.shadow_ubo->upload(&shadow_ubo, sizeof(shadow_ubo));

  const bool can_cull = m_meshlet_count > 0 && m_unique_meshlet_count > 0 &&
                       m_cull_pipeline != VK_NULL_HANDLE &&
                       m_emit_pipeline != VK_NULL_HANDLE;
  if (can_cull) {
    dispatchCull(cmd, frame, m_cull_sets[frame], buffers.cull_ubo.get(),
                 frame_state.projection * frame_state.view,
                 frame_state.camera_position, enable_hiz, false, false);
    dispatchEmit(cmd, frame, false);
    if (copy_hud_counts) {
      copyHudCounts(cmd, frame);
    }
  } else if (copy_hud_counts) {
    m_hud_copied_batches[frame] = 0;
  }
  cmdBufferBarrier(cmd, buffers.view_ubo->getBuffer(), VK_ACCESS_HOST_WRITE_BIT,
                   VK_ACCESS_UNIFORM_READ_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                       VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT |
                       VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT);
  cmdBufferBarrier(cmd, buffers.gbuffer_ubo->getBuffer(),
                   VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT,
                   VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  cmdBufferBarrier(cmd, buffers.shadow_ubo->getBuffer(),
                   VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
}

void GpuDrivenRenderer::recordShadowCull(VkCommandBuffer cmd, uint32_t frame,
                                         const ForwardFrameState& frame_state) {
  frame %= k_frames;
  if (m_meshlet_count == 0 || m_unique_meshlet_count == 0) {
    return;
  }
  dispatchCull(cmd, frame, m_shadow_cull_sets[frame],
               m_frames[frame].shadow_cull_ubo.get(),
               frame_state.light_view_projection, frame_state.camera_position,
               false, true, true);
  dispatchEmit(cmd, frame, true);
}

void GpuDrivenRenderer::writePbrDescriptors(uint32_t frame, ShadowMapTarget* shadow,
                                            VulkanTexture* fallback, bool late) {
  FrameBuffers& buffers = m_frames[frame];
  VkDescriptorBufferInfo ubo{};
  ubo.buffer = buffers.view_ubo->getBuffer();
  ubo.range = sizeof(ForwardMeshUniformData);
  VkDescriptorImageInfo shadow_image{};
  VkDescriptorImageInfo shadow_sampler{};
  if (shadow != nullptr) {
    shadow_image.imageView = shadow->getDepthImageView();
    shadow_image.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    shadow_sampler.sampler = shadow->getComparisonSampler();
  } else if (fallback != nullptr) {
    shadow_image.imageView = fallback->getImageView();
    shadow_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadow_sampler.sampler = fallback->getSampler();
  }
  VkDescriptorBufferInfo instances{};
  instances.buffer = buffers.instances->getBuffer();
  instances.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo compact{};
  compact.buffer = late ? buffers.late_compact_ids->getBuffer()
                        : buffers.early_compact_ids->getBuffer();
  compact.range = VK_WHOLE_SIZE;
  VkWriteDescriptorSet writes[5]{};
  for (uint32_t i = 0; i < 5; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = m_pbr_sets[frame];
    writes[i].descriptorCount = 1;
  }
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &ubo;
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo = &shadow_image;
  writes[2].dstBinding = 2;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[2].pImageInfo = &shadow_sampler;
  writes[3].dstBinding = 3;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[3].pBufferInfo = &instances;
  writes[4].dstBinding = 8;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[4].pBufferInfo = &compact;
  vkUpdateDescriptorSets(m_context->getDevice(), 5, writes, 0, nullptr);
  if (m_mesh_shadows != nullptr) {
    m_mesh_shadows->writeSamplingDescriptors(m_context->getDevice(),
                                            m_pbr_sets[frame], 4);
  }
}

void GpuDrivenRenderer::writeGBufferDescriptors(uint32_t frame, bool late) {
  if (m_gbuffer_sets[frame] == VK_NULL_HANDLE) {
    return;
  }
  FrameBuffers& buffers = m_frames[frame];
  VkDescriptorBufferInfo ubo{};
  ubo.buffer = buffers.gbuffer_ubo->getBuffer();
  ubo.range = sizeof(GpuDrivenGBufferUniformData);
  VkDescriptorBufferInfo instances{};
  instances.buffer = buffers.instances->getBuffer();
  instances.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo compact{};
  compact.buffer = late ? buffers.late_compact_ids->getBuffer()
                        : buffers.early_compact_ids->getBuffer();
  compact.range = VK_WHOLE_SIZE;
  VkWriteDescriptorSet writes[3]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_gbuffer_sets[frame];
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &ubo;
  writes[1] = writes[0];
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].pBufferInfo = &instances;
  writes[2] = writes[1];
  writes[2].dstBinding = 2;
  writes[2].pBufferInfo = &compact;
  vkUpdateDescriptorSets(m_context->getDevice(), 3, writes, 0, nullptr);
}

void GpuDrivenRenderer::writeShadowDescriptors(uint32_t frame) {
  if (m_shadow_sets[frame] == VK_NULL_HANDLE) {
    return;
  }
  FrameBuffers& buffers = m_frames[frame];
  VkDescriptorBufferInfo ubo{};
  ubo.buffer = buffers.shadow_ubo->getBuffer();
  ubo.range = sizeof(GpuDrivenShadowUniformData);
  VkDescriptorBufferInfo instances{};
  instances.buffer = buffers.instances->getBuffer();
  instances.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo compact{};
  compact.buffer = buffers.shadow_compact_ids->getBuffer();
  compact.range = VK_WHOLE_SIZE;
  VkWriteDescriptorSet writes[3]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = m_shadow_sets[frame];
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &ubo;
  writes[1] = writes[0];
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].pBufferInfo = &instances;
  writes[2] = writes[1];
  writes[2].dstBinding = 2;
  writes[2].pBufferInfo = &compact;
  vkUpdateDescriptorSets(m_context->getDevice(), 3, writes, 0, nullptr);
}

void GpuDrivenRenderer::recordIndirectBatches(
    VkCommandBuffer cmd, uint32_t frame, bool late, bool gbuffer, bool shadow,
    ShadowMapTarget* shadow_map, VulkanTexture* fallback) {
  if (m_packed_draws.empty() || m_meshlet_count == 0 || m_batches.empty()) {
    return;
  }
  FrameBuffers& buffers = m_frames[frame];
  VulkanPipeline* pipeline = shadow ? m_shadow_pipeline.get()
                                    : (gbuffer ? m_gbuffer_pipeline.get()
                                               : m_pbr_pipeline.get());
  if (pipeline == nullptr) {
    return;
  }
  if (!gbuffer && !shadow) {
    writePbrDescriptors(frame, shadow_map, fallback, late);
  } else if (gbuffer) {
    writeGBufferDescriptors(frame, late);
  } else {
    writeShadowDescriptors(frame);
  }
  VkDescriptorSet set = shadow ? m_shadow_sets[frame]
                               : (gbuffer ? m_gbuffer_sets[frame] : m_pbr_sets[frame]);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline->getGraphicsPipeline());
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline->getPipelineLayout(), 0, 1, &set, 0, nullptr);
  if (pipeline->usesBindlessTextureTable()) {
    VkDescriptorSet table = m_context->bindlessTextureTable().descriptorSet();
    if (table != VK_NULL_HANDLE) {
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipeline->getPipelineLayout(), 1, 1, &table, 0,
                              nullptr);
    }
  }

  VkBuffer cmd_buffer = shadow ? buffers.shadow_cmds->getBuffer()
                               : (late ? buffers.late_cmds->getBuffer()
                                       : buffers.early_cmds->getBuffer());
  VkBuffer count_buffer = shadow ? buffers.shadow_counts->getBuffer()
                                 : (late ? buffers.late_counts->getBuffer()
                                         : buffers.early_counts->getBuffer());
  const uint32_t stride = sizeof(DrawIndexedIndirectCommand);
  const bool compact = compactIndirectEnabled();
  const PFN_vkCmdDrawIndexedIndirectCount draw_count =
      compact ? m_context->cmdDrawIndexedIndirectCount() : nullptr;
  const bool multi_draw = m_context->multiDrawIndirectEnabled();
  const uint32_t max_indirect = m_context->maxDrawIndirectCount();
  uint32_t batch_i = 0;
  for (const MeshBatch& batch : m_batches) {
    if (batch_i >= k_max_mesh_batches) {
      break;
    }
    if (batch.mesh == nullptr || batch.meshlet_count == 0 ||
        batch.mesh->getMeshletIndexBuffer() == nullptr ||
        batch.mesh->getVertexBuffer() == nullptr) {
      ++batch_i;
      continue;
    }
    VkBuffer vb = batch.mesh->getVertexBuffer()->getBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, batch.mesh->getMeshletIndexBuffer()->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT32);
    const VkDeviceSize indirect_offset =
        static_cast<VkDeviceSize>(batch.meshlet_first) * stride;
    const uint32_t max_draws = eastl::min(batch.meshlet_count, max_indirect);
    if (draw_count != nullptr) {
      draw_count(cmd, cmd_buffer, indirect_offset, count_buffer,
                 static_cast<VkDeviceSize>(batch_i) * sizeof(uint32_t), max_draws,
                 stride);
    } else if (multi_draw) {
      vkCmdDrawIndexedIndirect(cmd, cmd_buffer, indirect_offset, batch.meshlet_count,
                               stride);
    } else {
      for (uint32_t i = 0; i < batch.meshlet_count; ++i) {
        vkCmdDrawIndexedIndirect(cmd, cmd_buffer, indirect_offset + i * stride, 1,
                                 stride);
      }
    }
    ++batch_i;
  }
}

void GpuDrivenRenderer::recordOpaqueIndirect(VkCommandBuffer cmd, uint32_t frame,
                                             const ForwardFrameState&, bool late,
                                             ShadowMapTarget* shadow,
                                             VulkanTexture* fallback) {
  frame %= k_frames;
  if (m_mesh_shaders_enabled && m_mesh_pipeline != VK_NULL_HANDLE &&
      m_context->cmdDrawMeshTasksEXT() != nullptr) {
    FrameBuffers& buffers = m_frames[frame];
    writePbrDescriptors(frame, shadow, fallback, late);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mesh_pipeline);
    VkDescriptorSet table = m_context->bindlessTextureTable().descriptorSet();
    uint32_t batch_i = 0;
    for (const MeshBatch& batch : m_batches) {
      if (batch_i >= k_max_mesh_batches) {
        break;
      }
      if (batch.mesh == nullptr ||
          batch.mesh->getMeshletVertexBuffer() == nullptr ||
          batch.mesh->getMeshletTriangleBuffer() == nullptr ||
          batch.mesh->getVertexBuffer() == nullptr) {
        ++batch_i;
        continue;
      }
      VkDescriptorSet set = m_mesh_sets[frame][late ? 1u : 0u][batch_i];
      VkDescriptorBufferInfo ubo{};
      ubo.buffer = buffers.view_ubo->getBuffer();
      ubo.range = sizeof(ForwardMeshUniformData);
      VkDescriptorImageInfo shadow_image{};
      VkDescriptorImageInfo shadow_sampler{};
      if (shadow != nullptr) {
        shadow_image.imageView = shadow->getDepthImageView();
        shadow_image.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadow_sampler.sampler = shadow->getComparisonSampler();
      } else if (fallback != nullptr) {
        shadow_image.imageView = fallback->getImageView();
        shadow_image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        shadow_sampler.sampler = fallback->getSampler();
      }
      VkDescriptorBufferInfo instances{};
      instances.buffer = buffers.instances->getBuffer();
      instances.range = VK_WHOLE_SIZE;
      VkDescriptorBufferInfo commands{};
      commands.buffer = late ? buffers.late_cmds->getBuffer()
                             : buffers.early_cmds->getBuffer();
      commands.offset = static_cast<VkDeviceSize>(batch.meshlet_first) *
                        sizeof(DrawIndexedIndirectCommand);
      commands.range = static_cast<VkDeviceSize>(batch.meshlet_count) *
                       sizeof(DrawIndexedIndirectCommand);
      VkDescriptorBufferInfo verts{};
      verts.buffer = batch.mesh->getVertexBuffer()->getBuffer();
      verts.range = VK_WHOLE_SIZE;
      VkDescriptorBufferInfo meshlet_verts{};
      meshlet_verts.buffer = batch.mesh->getMeshletVertexBuffer()->getBuffer();
      meshlet_verts.range = VK_WHOLE_SIZE;
      VkDescriptorBufferInfo meshlet_tris{};
      meshlet_tris.buffer = batch.mesh->getMeshletTriangleBuffer()->getBuffer();
      meshlet_tris.range = VK_WHOLE_SIZE;
      VkDescriptorBufferInfo meshlets{};
      meshlets.buffer = buffers.meshlets->getBuffer();
      meshlets.offset = static_cast<VkDeviceSize>(batch.expanded_first) *
                        sizeof(GpuDrivenMeshletGpu);
      meshlets.range = static_cast<VkDeviceSize>(batch.meshlet_count) *
                       sizeof(GpuDrivenMeshletGpu);
      VkDescriptorBufferInfo compact{};
      compact.buffer = late ? buffers.late_compact_ids->getBuffer()
                            : buffers.early_compact_ids->getBuffer();
      compact.range = VK_WHOLE_SIZE;
      VkWriteDescriptorSet writes[10]{};
      for (uint32_t i = 0; i < 10; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      }
      writes[0].dstBinding = 0;
      writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writes[0].pBufferInfo = &ubo;
      writes[1].dstBinding = 1;
      writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      writes[1].pImageInfo = &shadow_image;
      writes[2].dstBinding = 2;
      writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      writes[2].pImageInfo = &shadow_sampler;
      writes[3].dstBinding = 3;
      writes[3].pBufferInfo = &instances;
      writes[4].dstBinding = 4;
      writes[4].pBufferInfo = &commands;
      writes[5].dstBinding = 5;
      writes[5].pBufferInfo = &verts;
      writes[6].dstBinding = 6;
      writes[6].pBufferInfo = &meshlet_verts;
      writes[7].dstBinding = 7;
      writes[7].pBufferInfo = &meshlet_tris;
      writes[8].dstBinding = 8;
      writes[8].pBufferInfo = &meshlets;
      writes[9].dstBinding = 13;
      writes[9].pBufferInfo = &compact;
      vkUpdateDescriptorSets(m_context->getDevice(), 10, writes, 0, nullptr);
      if (m_mesh_shadows != nullptr) {
        m_mesh_shadows->writeSamplingDescriptors(m_context->getDevice(), set, 9);
      }
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_mesh_pipe_layout, 0, 1, &set, 0, nullptr);
      if (table != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_mesh_pipe_layout, 1, 1, &table, 0, nullptr);
      }
      m_context->cmdDrawMeshTasksEXT()(cmd, batch.meshlet_count, 1, 1);
      ++batch_i;
    }
    return;
  }
  recordIndirectBatches(cmd, frame, late, false, false, shadow, fallback);
}

void GpuDrivenRenderer::recordGBufferIndirect(VkCommandBuffer cmd, uint32_t frame,
                                              const ForwardFrameState&, bool late,
                                              VulkanTexture* fallback) {
  frame %= k_frames;
  recordIndirectBatches(cmd, frame, late, true, false, nullptr, fallback);
}

void GpuDrivenRenderer::recordShadowIndirect(VkCommandBuffer cmd, uint32_t frame,
                                             const ForwardFrameState&) {
  frame %= k_frames;
  recordIndirectBatches(cmd, frame, false, false, true, nullptr, nullptr);
}

void GpuDrivenRenderer::recordBuildHiZ(VkCommandBuffer cmd, uint32_t frame,
                                       VkImageView depth_view, uint32_t width,
                                       uint32_t height, VkImage depth_image) {
  frame %= k_frames;
  if (m_hiz_pipeline == VK_NULL_HANDLE || depth_view == VK_NULL_HANDLE) {
    return;
  }
  if (width != m_hiz_width || height != m_hiz_height ||
      m_hiz[frame].image == VK_NULL_HANDLE) {
    return;
  }
  HizPyramid& pyramid = m_hiz[frame];
  if (pyramid.image == VK_NULL_HANDLE) {
    return;
  }

  if (depth_image != VK_NULL_HANDLE) {
    VkImageMemoryBarrier depth_ready{};
    depth_ready.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depth_ready.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depth_ready.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    depth_ready.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depth_ready.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depth_ready.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depth_ready.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depth_ready.image = depth_image;
    depth_ready.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_ready.subresourceRange.levelCount = 1;
    depth_ready.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &depth_ready);
  }

  VkImageMemoryBarrier to_general{};
  to_general.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  to_general.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_general.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  to_general.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  to_general.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  to_general.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  to_general.image = pyramid.image;
  to_general.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  to_general.subresourceRange.levelCount = pyramid.mip_count;
  to_general.subresourceRange.layerCount = 1;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &to_general);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_hiz_pipeline);
  uint32_t src_w = width;
  uint32_t src_h = height;
  for (uint32_t mip = 0; mip < pyramid.mip_count; ++mip) {
    const uint32_t dst_w = eastl::max(1u, src_w / (mip == 0 ? 1u : 2u));
    const uint32_t dst_h = eastl::max(1u, src_h / (mip == 0 ? 1u : 2u));
    const uint32_t out_w = eastl::max(1u, pyramid.width >> mip);
    const uint32_t out_h = eastl::max(1u, pyramid.height >> mip);
    (void)dst_w;
    (void)dst_h;
    HizUniformData ubo{};
    ubo.src_width = mip == 0 ? width : eastl::max(1u, pyramid.width >> (mip - 1));
    ubo.src_height = mip == 0 ? height : eastl::max(1u, pyramid.height >> (mip - 1));
    ubo.dst_width = out_w;
    ubo.dst_height = out_h;
    ubo.src_is_depth = mip == 0 ? 1u : 0u;
    m_frames[frame].hiz_ubos[mip]->upload(&ubo, sizeof(ubo));

    VkDescriptorBufferInfo ubo_info{};
    ubo_info.buffer = m_frames[frame].hiz_ubos[mip]->getBuffer();
    ubo_info.range = sizeof(HizUniformData);
    VkDescriptorImageInfo src_info{};
    src_info.imageView = mip == 0 ? depth_view : pyramid.mip_views[mip - 1];
    src_info.imageLayout = mip == 0 ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                    : VK_IMAGE_LAYOUT_GENERAL;
    src_info.sampler = m_hiz_sampler;
    VkDescriptorImageInfo dst_info{};
    dst_info.imageView = pyramid.mip_views[mip];
    dst_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet writes[4]{};
    for (uint32_t i = 0; i < 4; ++i) {
      writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[i].dstSet = m_hiz_sets[frame][mip];
      writes[i].descriptorCount = 1;
    }
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &ubo_info;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &src_info;
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[2].pImageInfo = &src_info;
    writes[3].dstBinding = 3;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[3].pImageInfo = &dst_info;
    if (m_hiz_bound_src[frame][mip] != src_info.imageView ||
        m_hiz_bound_dst[frame][mip] != dst_info.imageView) {
      vkUpdateDescriptorSets(m_context->getDevice(), 4, writes, 0, nullptr);
      m_hiz_bound_src[frame][mip] = src_info.imageView;
      m_hiz_bound_dst[frame][mip] = dst_info.imageView;
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_hiz_pipe_layout, 0,
                            1, &m_hiz_sets[frame][mip], 0, nullptr);
    vkCmdDispatch(cmd, (out_w + 7u) / 8u, (out_h + 7u) / 8u, 1);
    VkImageMemoryBarrier mip_barrier = to_general;
    mip_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    mip_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    mip_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    mip_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    mip_barrier.subresourceRange.baseMipLevel = mip;
    mip_barrier.subresourceRange.levelCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &mip_barrier);
    src_w = out_w;
    src_h = out_h;
  }

  VkImageMemoryBarrier to_sampled = to_general;
  to_sampled.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  to_sampled.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  to_sampled.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
  to_sampled.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &to_sampled);

  if (depth_image != VK_NULL_HANDLE) {
    VkImageMemoryBarrier depth_release{};
    depth_release.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depth_release.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    depth_release.dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depth_release.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depth_release.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depth_release.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depth_release.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depth_release.image = depth_image;
    depth_release.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depth_release.subresourceRange.levelCount = 1;
    depth_release.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &depth_release);
  }
  pyramid.built = true;
}

}  // namespace Blunder
