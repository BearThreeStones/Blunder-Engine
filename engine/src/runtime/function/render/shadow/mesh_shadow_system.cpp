#include "runtime/function/render/shadow/mesh_shadow_system.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <glm/gtc/matrix_inverse.hpp>

#include "EASTL/algorithm.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_opaque_draw.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/shadow/local_shadow_math.h"
#include "runtime/function/render/slang/shader_resource_layout.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

namespace {

constexpr uint32_t k_shadow_mesh_sets = 1024u;
constexpr uint32_t k_shadow_ubo_slots = 16384u;
/// CPU frustum stamp covers receivers; GPU mark+download was a 320KB hitch
/// every orbit frame and is no longer required for courtyard coverage.
constexpr bool k_vsm_gpu_page_mark = false;

VkDescriptorType descType(ShaderDescriptorKind kind) {
  switch (kind) {
    case ShaderDescriptorKind::SampledImage:
      return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case ShaderDescriptorKind::Sampler:
      return VK_DESCRIPTOR_TYPE_SAMPLER;
    case ShaderDescriptorKind::StorageBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ShaderDescriptorKind::StorageImage:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    default:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }
}

void createCompareSampler(VkDevice device, float max_aniso, VkSampler* out) {
  VkSamplerCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.magFilter = VK_FILTER_LINEAR;
  info.minFilter = VK_FILTER_LINEAR;
  info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  info.compareEnable = VK_TRUE;
  info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  info.maxAnisotropy = max_aniso;
  if (vkCreateSampler(device, &info, nullptr, out) != VK_SUCCESS) {
    LOG_FATAL("[MeshShadowSystem] comparison sampler failed");
  }
}

}  // namespace

MeshShadowSystem::~MeshShadowSystem() { shutdown(); }

void MeshShadowSystem::initialize(VulkanContext* context, VulkanAllocator* allocator,
                                  SlangCompiler* compiler, bool full) {
  m_context = context;
  m_allocator = allocator;
  m_compiler = compiler;
  if (m_context == nullptr || m_allocator == nullptr) {
    return;
  }
  if (full && m_compiler == nullptr) {
    return;
  }
  m_mesh_shaders = full && m_context->meshShadersEnabled() &&
                   m_context->cmdDrawMeshTasksEXT() != nullptr;
  m_shader_output_layer = m_context->shaderOutputLayerEnabled();
  m_vsm_enabled = m_mesh_shaders;
  m_ubo_stride = 256;
  while (m_ubo_stride < sizeof(ShadowMeshUniformCpu)) {
    m_ubo_stride += 256;
  }
  m_flags_cpu.resize(k_vsm_virtual_pages, 0);
  m_page_table_cpu.resize(k_vsm_virtual_pages, k_vsm_unmarked_page);
  m_physical_capacity = k_vsm_physical_pages;
  if (m_context != nullptr) {
    const uint32_t max_layers = m_context->maxImageArrayLayers();
    if (max_layers > 0 && m_physical_capacity > max_layers) {
      m_physical_capacity = max_layers;
    }
  }
  m_physical_to_virtual.resize(m_physical_capacity, k_vsm_unmarked_page);
  if (full) {
    createDepthPass();
  }
  createDepthArray(m_dummy, 1, 1, 1, false);
  if (m_vsm_enabled) {
    createDepthArray(m_vsm, k_vsm_page_texels, k_vsm_page_texels,
                     m_physical_capacity, false);
  } else {
    createDepthArray(m_vsm, 1, 1, 1, false);
  }
  if (full) {
    createDepthArray(m_points, k_local_shadow_map_size, k_local_shadow_map_size,
                     k_max_point_shadow_maps * 6u, true);
    createDepthArray(m_spots, k_local_shadow_map_size, k_local_shadow_map_size,
                     k_max_spot_shadow_maps, false);
    createCompareSampler(m_context->getDevice(), m_context->getMaxSamplerAnisotropy(),
                         &m_comparison_sampler);
  } else {
    createDepthArray(m_points, 1, 1, 6, true);
    createDepthArray(m_spots, 1, 1, 1, false);
  }
  VkSamplerCreateInfo nearest{};
  nearest.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  nearest.magFilter = VK_FILTER_NEAREST;
  nearest.minFilter = VK_FILTER_NEAREST;
  nearest.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  nearest.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  nearest.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  vkCreateSampler(m_context->getDevice(), &nearest, nullptr, &m_depth_sampler);
  createBuffers();
  if (full) {
    createPipelines();
    createDescriptors();
  }
  {
    VkCommandBuffer cmd = m_context->beginImmediateCommands();
    auto barrier_to_read = [&](const DepthArray& target) {
      if (target.image == VK_NULL_HANDLE) {
        return;
      }
      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = target.image;
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.layerCount = target.layers;
      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &barrier);
    };
    barrier_to_read(m_dummy);
    barrier_to_read(m_vsm);
    barrier_to_read(m_points);
    barrier_to_read(m_spots);
    m_context->endImmediateCommands(cmd);
  }
  LOG_INFO(
      "[MeshShadowSystem] meshShaders={} shaderOutputLayer={} vsm={} "
      "pageTableBytes={} physicalPoolBytes={} full={}",
      m_mesh_shaders ? 1 : 0, m_shader_output_layer ? 1 : 0, m_vsm_enabled ? 1 : 0,
      static_cast<unsigned long long>(vsmPageTableBytes()),
      static_cast<unsigned long long>(vsmPhysicalPoolBytes()), full ? 1 : 0);
}

void MeshShadowSystem::shutdown() {
  destroyDescriptors();
  destroyPipelines();
  destroyBuffers();
  destroyDepthArray(m_dummy);
  destroyDepthArray(m_vsm);
  destroyDepthArray(m_points);
  destroyDepthArray(m_spots);
  destroyDepthPass();
  if (m_context != nullptr) {
    VkDevice device = m_context->getDevice();
    if (m_comparison_sampler != VK_NULL_HANDLE) {
      vkDestroySampler(device, m_comparison_sampler, nullptr);
      m_comparison_sampler = VK_NULL_HANDLE;
    }
    if (m_depth_sampler != VK_NULL_HANDLE) {
      vkDestroySampler(device, m_depth_sampler, nullptr);
      m_depth_sampler = VK_NULL_HANDLE;
    }
  }
  m_compiler = nullptr;
  m_allocator = nullptr;
  m_context = nullptr;
}

void MeshShadowSystem::createDepthPass() {
  VkAttachmentDescription depth{};
  depth.format = VK_FORMAT_D32_SFLOAT;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkAttachmentReference depth_ref{};
  depth_ref.attachment = 0;
  depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.pDepthStencilAttachment = &depth_ref;
  VkSubpassDependency deps[2]{};
  deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  deps[0].dstSubpass = 0;
  deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  deps[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  deps[1].srcSubpass = 0;
  deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
  deps[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = 1;
  info.pAttachments = &depth;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 2;
  info.pDependencies = deps;
  if (vkCreateRenderPass(m_context->getDevice(), &info, nullptr, &m_depth_pass) !=
      VK_SUCCESS) {
    LOG_FATAL("[MeshShadowSystem] depth render pass failed");
  }
}

void MeshShadowSystem::destroyDepthPass() {
  if (m_context != nullptr && m_depth_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(m_context->getDevice(), m_depth_pass, nullptr);
    m_depth_pass = VK_NULL_HANDLE;
  }
}

void MeshShadowSystem::createDepthArray(DepthArray& target, uint32_t width,
                                        uint32_t height, uint32_t layers,
                                        bool cube) {
  destroyDepthArray(target);
  target.width = width;
  target.height = height;
  target.layers = layers;
  target.cube = cube;
  VkImageCreateInfo image{};
  image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image.imageType = VK_IMAGE_TYPE_2D;
  image.format = VK_FORMAT_D32_SFLOAT;
  image.extent = {width, height, 1};
  image.mipLevels = 1;
  image.arrayLayers = layers;
  image.samples = VK_SAMPLE_COUNT_1_BIT;
  image.tiling = VK_IMAGE_TILING_OPTIMAL;
  image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT;
  image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (cube) {
    image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }
  VmaAllocationCreateInfo alloc{};
  alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  if (vmaCreateImage(m_allocator->getAllocator(), &image, &alloc, &target.image,
                     &target.allocation, nullptr) != VK_SUCCESS) {
    LOG_FATAL("[MeshShadowSystem] depth array image failed");
  }
  VkDevice device = m_context->getDevice();
  VkImageViewCreateInfo view{};
  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.image = target.image;
  view.viewType = cube ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  view.format = VK_FORMAT_D32_SFLOAT;
  view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  view.subresourceRange.levelCount = 1;
  view.subresourceRange.layerCount = layers;
  if (vkCreateImageView(device, &view, nullptr, &target.array_view) != VK_SUCCESS) {
    LOG_FATAL("[MeshShadowSystem] array view failed");
  }
  if (cube) {
    view.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    view.subresourceRange.layerCount = layers;
    if (vkCreateImageView(device, &view, nullptr, &target.cube_view) != VK_SUCCESS) {
      LOG_FATAL("[MeshShadowSystem] cube view failed");
    }
  }
  target.layer_views.resize(layers);
  target.layer_fbs.resize(layers, VK_NULL_HANDLE);
  if (m_depth_pass != VK_NULL_HANDLE) {
    for (uint32_t i = 0; i < layers; ++i) {
      view.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view.subresourceRange.baseArrayLayer = i;
      view.subresourceRange.layerCount = 1;
      if (vkCreateImageView(device, &view, nullptr, &target.layer_views[i]) !=
          VK_SUCCESS) {
        LOG_FATAL("[MeshShadowSystem] layer view failed");
      }
      VkFramebufferCreateInfo fb{};
      fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      fb.renderPass = m_depth_pass;
      fb.attachmentCount = 1;
      fb.pAttachments = &target.layer_views[i];
      fb.width = width;
      fb.height = height;
      fb.layers = 1;
      if (vkCreateFramebuffer(device, &fb, nullptr, &target.layer_fbs[i]) !=
          VK_SUCCESS) {
        LOG_FATAL("[MeshShadowSystem] layer framebuffer failed");
      }
    }
    if (cube && layers >= 6) {
      const uint32_t slots = layers / 6u;
      target.slot_views.resize(slots, VK_NULL_HANDLE);
      target.slot_fbs.resize(slots, VK_NULL_HANDLE);
      for (uint32_t s = 0; s < slots; ++s) {
        view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        view.subresourceRange.baseArrayLayer = s * 6u;
        view.subresourceRange.layerCount = 6;
        if (vkCreateImageView(device, &view, nullptr, &target.slot_views[s]) !=
            VK_SUCCESS) {
          LOG_FATAL("[MeshShadowSystem] cube slot view failed");
        }
        VkFramebufferCreateInfo fb{};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = m_depth_pass;
        fb.attachmentCount = 1;
        fb.pAttachments = &target.slot_views[s];
        fb.width = width;
        fb.height = height;
        fb.layers = 6;
        if (vkCreateFramebuffer(device, &fb, nullptr, &target.slot_fbs[s]) !=
            VK_SUCCESS) {
          LOG_WARN("[MeshShadowSystem] cube slot framebuffer failed; six-pass");
          target.slot_fbs[s] = VK_NULL_HANDLE;
        }
      }
    }
  } else {
    for (uint32_t i = 0; i < layers; ++i) {
      target.layer_views[i] = VK_NULL_HANDLE;
    }
  }
}

void MeshShadowSystem::destroyDepthArray(DepthArray& target) {
  if (m_context == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  if (target.layered_fb != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, target.layered_fb, nullptr);
    target.layered_fb = VK_NULL_HANDLE;
  }
  for (VkFramebuffer fb : target.slot_fbs) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
  }
  target.slot_fbs.clear();
  for (VkImageView view : target.slot_views) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, view, nullptr);
    }
  }
  target.slot_views.clear();
  for (VkFramebuffer fb : target.layer_fbs) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(device, fb, nullptr);
    }
  }
  target.layer_fbs.clear();
  for (VkImageView view : target.layer_views) {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, view, nullptr);
    }
  }
  target.layer_views.clear();
  if (target.cube_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, target.cube_view, nullptr);
    target.cube_view = VK_NULL_HANDLE;
  }
  if (target.array_view != VK_NULL_HANDLE) {
    vkDestroyImageView(device, target.array_view, nullptr);
    target.array_view = VK_NULL_HANDLE;
  }
  if (target.image != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator->getAllocator(), target.image, target.allocation);
    target.image = VK_NULL_HANDLE;
    target.allocation = VK_NULL_HANDLE;
  }
}

void MeshShadowSystem::createBuffers() {
  auto make_buf = [this](VkDeviceSize size, VkBufferUsageFlags usage,
                         VmaMemoryUsage mem) {
    auto buf = eastl::make_unique<VulkanBuffer>();
    buf->create(m_allocator, size, usage, mem);
    return buf;
  };
  m_page_table = make_buf(vsmPageTableBytes(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                          VMA_MEMORY_USAGE_CPU_TO_GPU);
  for (uint32_t frame = 0; frame < k_frames; ++frame) {
    m_page_flags_gpu[frame] = make_buf(
        vsmPageTableBytes(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    m_page_flags_staging[frame] = make_buf(
        vsmPageTableBytes(), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_TO_CPU);
    m_mark_ubos[frame] = make_buf(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VMA_MEMORY_USAGE_CPU_TO_GPU);
  }
  m_instances = make_buf(sizeof(GpuDrivenInstanceGpu) * k_max_gpu_driven_instances,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_meshlets = make_buf(sizeof(GpuDrivenMeshletGpu) * k_max_gpu_driven_meshlets,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_shadow_ubo = make_buf(static_cast<VkDeviceSize>(m_ubo_stride) * k_shadow_ubo_slots,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          VMA_MEMORY_USAGE_CPU_TO_GPU);
  eastl::vector<uint32_t> unmarked(k_vsm_virtual_pages, k_vsm_unmarked_page);
  m_page_table->upload(unmarked.data(), vsmPageTableBytes());
  eastl::vector<uint32_t> zero_flags(k_vsm_virtual_pages, 0u);
  for (uint32_t frame = 0; frame < k_frames; ++frame) {
    if (m_page_flags_staging[frame] != nullptr) {
      m_page_flags_staging[frame]->upload(zero_flags.data(), vsmPageTableBytes());
    }
  }
}

void MeshShadowSystem::destroyBuffers() {
  auto drop = [](eastl::unique_ptr<VulkanBuffer>& buf) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  };
  drop(m_page_table);
  for (uint32_t frame = 0; frame < k_frames; ++frame) {
    drop(m_page_flags_gpu[frame]);
    drop(m_page_flags_staging[frame]);
    drop(m_mark_ubos[frame]);
  }
  drop(m_instances);
  drop(m_meshlets);
  drop(m_shadow_ubo);
}

void MeshShadowSystem::createPipelines() {
  if (!m_mesh_shaders) {
    VulkanPipelineCreateInfo vs{};
    vs.shader_path = "engine/shaders/shadow_depth.slang";
    vs.enable_vertex_input = true;
    vs.cull_mode = VK_CULL_MODE_NONE;
    vs.enable_depth_test = true;
    vs.enable_depth_write = true;
    vs.enable_depth_bias = true;
    vs.depth_bias_constant_factor = 1.25f;
    vs.depth_bias_slope_factor = 1.75f;
    vs.depth_only_subpass = true;
    uint32_t count = 0;
    fillSequentialExpectedBindings(vs.expected_descriptor_bindings, &count, 1);
    vs.expected_descriptor_binding_count = count;
    vs.expected_descriptor_kinds[0] = ShaderDescriptorKind::UniformBuffer;
    m_vs_pipeline = eastl::make_unique<VulkanPipeline>();
    m_vs_pipeline->initialize(m_context, m_compiler, m_depth_pass, vs);
    return;
  }

  auto make_compute = [&](const char* path, uint32_t binding_count,
                          const uint32_t* bindings, const ShaderDescriptorKind* kinds,
                          VkDescriptorSetLayout* layout, VkPipelineLayout* pipe_layout,
                          VkPipeline* pipeline) {
    const auto program = m_compiler->compileComputeProgram(path, "main");
    uint32_t sets[8]{};
    if (!shaderResourceBindingsMatch(program.layout, bindings, binding_count, sets,
                                     kinds)) {
      LOG_FATAL("[MeshShadowSystem] layout mismatch {}", path);
    }
    eastl::vector<VkDescriptorSetLayoutBinding> layout_bindings;
    for (uint32_t i = 0; i < program.layout.count; ++i) {
      if (program.layout.bindings[i].set != 0) {
        continue;
      }
      VkDescriptorSetLayoutBinding b{};
      b.binding = program.layout.bindings[i].binding;
      b.descriptorType = descType(program.layout.bindings[i].kind);
      b.descriptorCount = 1;
      b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      layout_bindings.push_back(b);
    }
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
    layout_info.pBindings = layout_bindings.data();
    vkCreateDescriptorSetLayout(m_context->getDevice(), &layout_info, nullptr,
                                layout);
    VkPipelineLayoutCreateInfo pipe_info{};
    pipe_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipe_info.setLayoutCount = 1;
    pipe_info.pSetLayouts = layout;
    vkCreatePipelineLayout(m_context->getDevice(), &pipe_info, nullptr, pipe_layout);
    VkShaderModuleCreateInfo module_info{};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.codeSize = program.compute.spirv_code.size();
    module_info.pCode =
        reinterpret_cast<const uint32_t*>(program.compute.spirv_code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule(m_context->getDevice(), &module_info, nullptr, &module);
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = module;
    stage.pName = program.compute.entry_point_name.c_str();
    VkComputePipelineCreateInfo cp{};
    cp.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp.stage = stage;
    cp.layout = *pipe_layout;
    vkCreateComputePipelines(m_context->getDevice(), VK_NULL_HANDLE, 1, &cp, nullptr,
                             pipeline);
    vkDestroyShaderModule(m_context->getDevice(), module, nullptr);
  };

  uint32_t mark_bindings[] = {0, 1, 2};
  ShaderDescriptorKind mark_kinds[] = {
      ShaderDescriptorKind::UniformBuffer, ShaderDescriptorKind::SampledImage,
      ShaderDescriptorKind::StorageBuffer};
  make_compute("engine/shaders/vsm_page_mark.slang", 3, mark_bindings, mark_kinds,
               &m_mark_layout, &m_mark_pipe_layout, &m_mark_pipeline);

  const auto mesh_program =
      m_compiler->compileMeshProgram("engine/shaders/shadow_mesh.slang");
  uint32_t mesh_bindings[] = {0, 1, 2, 3, 4, 5};
  uint32_t mesh_sets[] = {0, 0, 0, 0, 0, 0};
  ShaderDescriptorKind mesh_kinds[] = {
      ShaderDescriptorKind::UniformBuffer, ShaderDescriptorKind::StorageBuffer,
      ShaderDescriptorKind::StorageBuffer, ShaderDescriptorKind::StorageBuffer,
      ShaderDescriptorKind::StorageBuffer, ShaderDescriptorKind::StorageBuffer};
  if (!shaderResourceBindingsMatch(mesh_program.layout, mesh_bindings, 6, mesh_sets,
                                   mesh_kinds)) {
    LOG_FATAL("[MeshShadowSystem] shadow_mesh.slang layout mismatch");
  }
  eastl::vector<VkDescriptorSetLayoutBinding> mesh_lb;
  for (uint32_t i = 0; i < mesh_program.layout.count; ++i) {
    if (mesh_program.layout.bindings[i].set != 0) {
      continue;
    }
    VkDescriptorSetLayoutBinding b{};
    b.binding = mesh_program.layout.bindings[i].binding;
    b.descriptorType = mesh_program.layout.bindings[i].kind ==
                               ShaderDescriptorKind::UniformBuffer
                           ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                           : descType(mesh_program.layout.bindings[i].kind);
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT |
                   VK_SHADER_STAGE_FRAGMENT_BIT;
    mesh_lb.push_back(b);
  }
  VkDescriptorSetLayoutCreateInfo mesh_layout_info{};
  mesh_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  mesh_layout_info.bindingCount = static_cast<uint32_t>(mesh_lb.size());
  mesh_layout_info.pBindings = mesh_lb.data();
  vkCreateDescriptorSetLayout(m_context->getDevice(), &mesh_layout_info, nullptr,
                              &m_mesh_layout);
  VkPushConstantRange mesh_push{};
  mesh_push.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT;
  mesh_push.offset = 0;
  mesh_push.size = 16;
  VkPipelineLayoutCreateInfo mesh_pipe_info{};
  mesh_pipe_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  mesh_pipe_info.setLayoutCount = 1;
  mesh_pipe_info.pSetLayouts = &m_mesh_layout;
  mesh_pipe_info.pushConstantRangeCount = 1;
  mesh_pipe_info.pPushConstantRanges = &mesh_push;
  vkCreatePipelineLayout(m_context->getDevice(), &mesh_pipe_info, nullptr,
                         &m_mesh_pipe_layout);

  auto make_module = [&](const eastl::vector<uint8_t>& spirv) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spirv.size();
    info.pCode = reinterpret_cast<const uint32_t*>(spirv.data());
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule(m_context->getDevice(), &info, nullptr, &module);
    return module;
  };
  auto make_mesh_pipe = [&](const char* mesh_entry, VkPipeline* out) {
    const auto program = m_compiler->compileMeshProgram(
        "engine/shaders/shadow_mesh.slang", "taskMain", mesh_entry, "fragmentMain");
    VkShaderModule task = make_module(program.task.spirv_code);
    VkShaderModule mesh = make_module(program.mesh.spirv_code);
    VkShaderModule frag = make_module(program.fragment.spirv_code);
    VkPipelineShaderStageCreateInfo stages[3]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    stages[0].module = task;
    stages[0].pName = program.task.entry_point_name.c_str();
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    stages[1].module = mesh;
    stages[1].pName = program.mesh.entry_point_name.c_str();
    stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[2].module = frag;
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
    raster.depthBiasEnable = VK_TRUE;
    raster.depthBiasConstantFactor = 1.25f;
    raster.depthBiasSlopeFactor = 1.75f;
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 0;
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
    gp.renderPass = m_depth_pass;
    const VkResult result = m_context->createGraphicsPipelines(1, &gp, out);
    vkDestroyShaderModule(m_context->getDevice(), task, nullptr);
    vkDestroyShaderModule(m_context->getDevice(), mesh, nullptr);
    vkDestroyShaderModule(m_context->getDevice(), frag, nullptr);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[MeshShadowSystem] mesh pipeline '{}' failed: {}", mesh_entry,
                static_cast<int>(result));
    }
  };
  make_mesh_pipe("meshMain", &m_mesh_pipeline);
  if (m_shader_output_layer) {
    make_mesh_pipe("meshMainLayered", &m_mesh_layered_pipeline);
  }
}

void MeshShadowSystem::destroyPipelines() {
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
  auto drop_set = [&](VkDescriptorSetLayout& p) {
    if (device != VK_NULL_HANDLE && p != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(device, p, nullptr);
      p = VK_NULL_HANDLE;
    }
  };
  drop_pipe(m_mesh_pipeline);
  drop_pipe(m_mesh_layered_pipeline);
  drop_pipe(m_mark_pipeline);
  drop_layout(m_mesh_pipe_layout);
  drop_layout(m_mark_pipe_layout);
  drop_set(m_mesh_layout);
  drop_set(m_mark_layout);
  if (m_vs_pipeline) {
    m_vs_pipeline->shutdown();
    m_vs_pipeline.reset();
  }
}

void MeshShadowSystem::createDescriptors() {
  VkDevice device = m_context->getDevice();
  const uint32_t mesh_sets =
      m_mesh_layout != VK_NULL_HANDLE ? k_shadow_mesh_sets * k_frames : 0u;
  VkDescriptorPoolSize sizes[4]{};
  sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  sizes[0].descriptorCount = mesh_sets + 8;
  sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  sizes[1].descriptorCount = 8 + k_frames;
  sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  sizes[2].descriptorCount = mesh_sets * 5u + 16u + k_frames;
  sizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  sizes[3].descriptorCount = 8 + k_frames;
  VkDescriptorPoolSize extra_sampler{};
  extra_sampler.type = VK_DESCRIPTOR_TYPE_SAMPLER;
  extra_sampler.descriptorCount = 8;
  VkDescriptorPoolSize pool_sizes[5] = {sizes[0], sizes[1], sizes[2], sizes[3],
                                        extra_sampler};
  VkDescriptorPoolCreateInfo pool{};
  pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool.poolSizeCount = 5;
  pool.pPoolSizes = pool_sizes;
  pool.maxSets = mesh_sets + 8 + k_frames;
  vkCreateDescriptorPool(device, &pool, nullptr, &m_descriptor_pool);
  auto alloc = [&](VkDescriptorSetLayout layout, VkDescriptorSet* out) {
    if (layout == VK_NULL_HANDLE) {
      return;
    }
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = m_descriptor_pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;
    vkAllocateDescriptorSets(device, &info, out);
  };
  m_mesh_sets.clear();
  if (m_mesh_layout != VK_NULL_HANDLE && mesh_sets > 0) {
    eastl::vector<VkDescriptorSetLayout> layouts(mesh_sets, m_mesh_layout);
    m_mesh_sets.resize(mesh_sets);
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = m_descriptor_pool;
    info.descriptorSetCount = mesh_sets;
    info.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(device, &info, m_mesh_sets.data()) != VK_SUCCESS) {
      LOG_FATAL("[MeshShadowSystem] mesh descriptor sets failed");
    }
  }
  if (m_mark_layout != VK_NULL_HANDLE) {
    for (uint32_t frame = 0; frame < k_frames; ++frame) {
      alloc(m_mark_layout, &m_mark_sets[frame]);
    }
  }
  if (m_vs_pipeline != nullptr) {
    alloc(m_vs_pipeline->getDescriptorSetLayout(), &m_vs_set);
  }
}

void MeshShadowSystem::destroyDescriptors() {
  if (m_context != nullptr && m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_context->getDevice(), m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
  }
  m_mesh_sets.clear();
  for (uint32_t frame = 0; frame < k_frames; ++frame) {
    m_mark_sets[frame] = VK_NULL_HANDLE;
  }
  m_vs_set = VK_NULL_HANDLE;
}

void MeshShadowSystem::uploadCasters(const GpuDrivenDraw* draws, uint32_t count,
                                     bool scene_static) {
  if (scene_static && m_casters_ready && count == m_last_caster_draw_count) {
    if (!m_mesh_sets_written[m_record_frame]) {
      writeInstanceMeshSets();
      m_mesh_sets_written[m_record_frame] = true;
    }
    return;
  }
  collectOpaqueMeshletCasters(draws, count, m_casters);
  m_instance_cpu.clear();
  m_meshlet_cpu.clear();
  m_instance_meshes.clear();
  m_instance_count = 0;
  m_meshlet_count = 0;
  for (const MeshShadowCasterDraw& caster : m_casters) {
    if (caster.draw == nullptr || caster.draw->gpu_mesh == nullptr) {
      continue;
    }
    if (m_instance_count >= k_max_gpu_driven_instances) {
      break;
    }
    const auto& records = caster.draw->gpu_mesh->getMeshletRecords();
    GpuDrivenInstanceGpu inst{};
    inst.world = caster.draw->model;
    inst.meshlet_offset = m_meshlet_count;
    inst.meshlet_count = static_cast<uint32_t>(records.size());
    inst.pad0 = caster.draw->entity_id;
    m_instance_cpu.push_back(inst);
    m_instance_meshes.push_back(caster.draw->gpu_mesh);
    for (const GpuMeshletGpuRecord& rec : records) {
      if (m_meshlet_count >= k_max_gpu_driven_meshlets) {
        break;
      }
      GpuDrivenMeshletGpu meshlet{};
      meshlet.sphere = rec.sphere;
      meshlet.cone = rec.cone;
      meshlet.instance_index = m_instance_count;
      meshlet.vertex_offset = rec.vertex_offset;
      meshlet.vertex_count = rec.vertex_count;
      meshlet.triangle_offset = rec.triangle_offset;
      meshlet.triangle_count = rec.triangle_count;
      m_meshlet_cpu.push_back(meshlet);
      ++m_meshlet_count;
    }
    ++m_instance_count;
  }
  if (m_instance_count > 0) {
    m_instances->upload(m_instance_cpu.data(),
                        sizeof(GpuDrivenInstanceGpu) * m_instance_count);
  }
  if (m_meshlet_count > 0) {
    m_meshlets->upload(m_meshlet_cpu.data(),
                       sizeof(GpuDrivenMeshletGpu) * m_meshlet_count);
  }
  writeInstanceMeshSets();
  for (bool& written : m_mesh_sets_written) {
    written = false;
  }
  m_mesh_sets_written[m_record_frame] = true;
  m_casters_ready = true;
  m_last_caster_draw_count = count;
}

void MeshShadowSystem::uploadUboSlot(uint32_t slot, const void* data,
                                     VkDeviceSize size) {
  if (m_shadow_ubo == nullptr || data == nullptr || m_allocator == nullptr) {
    return;
  }
  void* mapped = nullptr;
  if (vmaMapMemory(m_allocator->getAllocator(), m_shadow_ubo->getAllocation(),
                   &mapped) != VK_SUCCESS) {
    LOG_FATAL("[MeshShadowSystem] UBO map failed");
  }
  const VkDeviceSize offset = static_cast<VkDeviceSize>(slot) * m_ubo_stride;
  std::memcpy(static_cast<uint8_t*>(mapped) + offset, data,
              static_cast<size_t>(size));
  vmaFlushAllocation(m_allocator->getAllocator(), m_shadow_ubo->getAllocation(),
                     offset, size);
  vmaUnmapMemory(m_allocator->getAllocator(), m_shadow_ubo->getAllocation());
}

void MeshShadowSystem::writeInstanceMeshSets() {
  if (m_mesh_sets.empty() || m_shadow_ubo == nullptr) {
    return;
  }
  VkDevice device = m_context->getDevice();
  const uint32_t set_base = m_record_frame * k_shadow_mesh_sets;
  const uint32_t n = eastl::min(
      m_instance_count,
      eastl::min(k_shadow_mesh_sets,
                 static_cast<uint32_t>(m_mesh_sets.size() > set_base
                                           ? m_mesh_sets.size() - set_base
                                           : 0u)));
  VkDescriptorBufferInfo inst{};
  inst.buffer = m_instances->getBuffer();
  inst.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo meshlets{};
  meshlets.buffer = m_meshlets->getBuffer();
  meshlets.range = VK_WHOLE_SIZE;
  for (uint32_t i = 0; i < n; ++i) {
    GpuMesh* mesh = i < m_instance_meshes.size() ? m_instance_meshes[i] : nullptr;
    if (mesh == nullptr || mesh->getVertexBuffer() == nullptr ||
        mesh->getMeshletVertexBuffer() == nullptr ||
        mesh->getMeshletTriangleBuffer() == nullptr) {
      continue;
    }
    VkDescriptorBufferInfo ubo_info{};
    ubo_info.buffer = m_shadow_ubo->getBuffer();
    ubo_info.range = m_ubo_stride;
    VkDescriptorBufferInfo verts{};
    verts.buffer = mesh->getVertexBuffer()->getBuffer();
    verts.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo mverts{};
    mverts.buffer = mesh->getMeshletVertexBuffer()->getBuffer();
    mverts.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo mtris{};
    mtris.buffer = mesh->getMeshletTriangleBuffer()->getBuffer();
    mtris.range = VK_WHOLE_SIZE;
    VkWriteDescriptorSet writes[6]{};
    for (uint32_t w = 0; w < 6; ++w) {
      writes[w].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[w].dstSet = m_mesh_sets[set_base + i];
      writes[w].descriptorCount = 1;
      writes[w].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[0].pBufferInfo = &ubo_info;
    writes[1].dstBinding = 1;
    writes[1].pBufferInfo = &inst;
    writes[2].dstBinding = 2;
    writes[2].pBufferInfo = &meshlets;
    writes[3].dstBinding = 3;
    writes[3].pBufferInfo = &verts;
    writes[4].dstBinding = 4;
    writes[4].pBufferInfo = &mverts;
    writes[5].dstBinding = 5;
    writes[5].pBufferInfo = &mtris;
    vkUpdateDescriptorSets(device, 6, writes, 0, nullptr);
  }
}

void MeshShadowSystem::stampCameraPages() {
  for (uint32_t virt : m_stamp_virt) {
    if (virt < m_flags_cpu.size()) {
      m_flags_cpu[virt] = 0u;
    }
  }
  m_stamp_virt.clear();
  auto stamp_world = [&](const glm::vec3& world) {
    for (uint32_t level = k_vsm_first_level; level <= k_vsm_last_level; ++level) {
      uint32_t index = 0;
      if (!vsmWorldPageIndex(m_vsm_light_view, world, level, &index) ||
          index >= m_flags_cpu.size() || m_flags_cpu[index] != 0u) {
        continue;
      }
      m_flags_cpu[index] = 1u;
      m_stamp_virt.push_back(index);
    }
  };
  stamp_world(m_camera);
  // 7x7 around the eye in light XY (near geometry / camera-adjacent floor).
  const glm::vec3 axis_x(m_vsm_light_view[0][0], m_vsm_light_view[1][0],
                         m_vsm_light_view[2][0]);
  const glm::vec3 axis_y(m_vsm_light_view[0][1], m_vsm_light_view[1][1],
                         m_vsm_light_view[2][1]);
  for (uint32_t level = k_vsm_first_level; level <= k_vsm_last_level; ++level) {
    const float page = (2.0f * vsmClipmapRadius(level)) /
                       static_cast<float>(k_vsm_pages_per_axis);
    for (int iy = -3; iy <= 3; ++iy) {
      for (int ix = -3; ix <= 3; ++ix) {
        stamp_world(m_camera + axis_x * (page * static_cast<float>(ix)) +
                    axis_y * (page * static_cast<float>(iy)));
      }
    }
  }
  // Visible frustum: looking down the courtyard the floor is 8–16 m ahead of
  // the eye, outside the 7x7 at level 6. Unproject a screen grid so those
  // receivers are marked this frame instead of waiting on GPU page-mark lag.
  const float zs[] = {0.0f, 0.12f, 0.28f, 0.5f, 0.75f, 0.96f};
  const float nys[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
  const float nxs[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
  for (float z : zs) {
    for (float y : nys) {
      for (float x : nxs) {
        const glm::vec4 clip(x, y, z, 1.0f);
        const glm::vec4 world4 = m_inv_view_projection * clip;
        if (std::abs(world4.w) < 1e-6f) {
          continue;
        }
        stamp_world(glm::vec3(world4) / world4.w);
      }
    }
  }
}

void MeshShadowSystem::mergeGpuMarksFromStaging(uint32_t frame) {
  m_gpu_mark_count = 0;
  if (frame >= k_frames || m_page_flags_staging[frame] == nullptr ||
      m_flags_cpu.size() < k_vsm_virtual_pages) {
    return;
  }
  eastl::vector<uint32_t> gpu(k_vsm_virtual_pages, 0u);
  if (!m_page_flags_staging[frame]->download(gpu.data(), vsmPageTableBytes())) {
    return;
  }
  for (uint32_t i = 0; i < k_vsm_virtual_pages; ++i) {
    if (gpu[i] != 0u) {
      m_flags_cpu[i] = 1u;
      ++m_gpu_mark_count;
    }
  }
}

void MeshShadowSystem::barrierDepthArrayToShaderRead(VkCommandBuffer cmd,
                                                     const DepthArray& target,
                                                     uint32_t layers) {
  if (cmd == VK_NULL_HANDLE || target.image == VK_NULL_HANDLE || layers == 0) {
    return;
  }
  const uint32_t count = eastl::min(layers, target.layers);
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = target.image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = count;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

void MeshShadowSystem::logVsmFrame() {
  const char* debug = std::getenv("BLUNDER_DEBUG_VSM");
  if (debug == nullptr || debug[0] == '\0' || debug[0] == '0') {
    return;
  }
  ++m_debug_frame;
  if (m_debug_frame > 8u && (m_debug_frame % 120u) != 0u) {
    return;
  }
  auto probe = [&](const glm::vec3& world, uint32_t* virt, uint32_t* slot,
                   float* clip_z) {
    *virt = k_vsm_unmarked_page;
    *slot = k_vsm_unmarked_page;
    *clip_z = -1.0f;
    const glm::vec3 light_pos = vsmWorldToLight(m_vsm_light_view, world);
    *clip_z = vsmLightSpaceClipDepth(light_pos.z);
    for (uint32_t level = k_vsm_first_level; level <= k_vsm_last_level; ++level) {
      uint32_t page_x = 0;
      uint32_t page_y = 0;
      glm::vec2 uv(0.0f);
      if (!vsmLightToPage(light_pos, level, page_x, page_y, uv)) {
        continue;
      }
      const uint32_t index = vsmVirtualPageIndex(level, page_x, page_y);
      if (index < m_page_table_cpu.size() &&
          m_page_table_cpu[index] != k_vsm_unmarked_page) {
        *virt = index;
        *slot = m_page_table_cpu[index];
        return;
      }
    }
  };
  uint32_t floor_virt = k_vsm_unmarked_page;
  uint32_t floor_slot = k_vsm_unmarked_page;
  float floor_z = -1.0f;
  uint32_t mid_virt = k_vsm_unmarked_page;
  uint32_t mid_slot = k_vsm_unmarked_page;
  float mid_z = -1.0f;
  probe(glm::vec3(0.0f, 0.0f, 0.0f), &floor_virt, &floor_slot, &floor_z);
  probe(glm::vec3(0.0f, -4.0f, 0.0f), &mid_virt, &mid_slot, &mid_z);
  LOG_INFO(
      "[MeshShadowSystem] frame={} vsm={} dir={} meshlets={} marked={} "
      "gpuMarks={} overflow={} light=({:.3f},{:.3f},{:.3f}) cam=({:.3f},{:.3f},"
      "{:.3f}) floorVirt={} floorSlot={} floorZ={:.4f} midVirt={} midSlot={} "
      "midZ={:.4f}",
      m_debug_frame, m_vsm_enabled ? 1 : 0, isValid(m_local.directional) ? 1 : 0,
      m_meshlet_count, m_marked_count, m_gpu_mark_count, m_overflow, m_light_dir.x,
      m_light_dir.y, m_light_dir.z, m_camera.x, m_camera.y, m_camera.z, floor_virt,
      floor_slot, floor_z, mid_virt, mid_slot, mid_z);
}

void MeshShadowSystem::compactCpuFlags() {
  const VsmCompactResult compact = vsmCompactMarkedPages(
      m_stamp_virt.data(), static_cast<uint32_t>(m_stamp_virt.size()),
      m_page_table_cpu.data(), m_physical_to_virtual.data(),
      m_physical_capacity);
  m_marked_count = compact.assigned;
  m_overflow = compact.overflow;
  m_page_table->upload(m_page_table_cpu.data(), vsmPageTableBytes());
  if (m_overflow > 0 && !m_logged_page_overflow) {
    LOG_WARN("[MeshShadowSystem] VSM page pool overflow dropped {} pages",
             m_overflow);
    m_logged_page_overflow = true;
  }
}

void MeshShadowSystem::beginFrame(const ForwardFrameState& frame_state,
                                  const GpuDrivenDraw* gpu_draws,
                                  uint32_t gpu_draw_count, uint32_t frame_index) {
  m_record_frame = frame_index % k_frames;
  if (!frame_state.scene_static) {
    m_local = {};
    if (frame_state.lighting_scene != nullptr) {
      m_local = pickLocalShadowCasters(*frame_state.lighting_scene);
    } else if (isValid(frame_state.shadow_caster_id)) {
      m_local.directional = frame_state.shadow_caster_id;
    }
    m_light_dir = glm::vec3(0.45f, 0.7f, 0.55f);
    if (frame_state.lighting_scene != nullptr && isValid(m_local.directional)) {
      const Vec3 emit = lightWorldEmit(
          frame_state.lighting_scene->getWorldMatrix(m_local.directional));
      m_light_dir = lightShadingL(LightType::directional, emit, Vec3(0.0f), Vec3(0.0f));
    }
    if (frame_state.lighting_scene != nullptr) {
      // Spot far plane is the metre-authored range measured in world units.
      const float world_per_metre =
          sceneWorldUnitsPerMetre(*frame_state.lighting_scene);
      for (uint32_t i = 0; i < m_local.spot_count; ++i) {
        const EntityId id = m_local.spots[i];
        const LightComponent* light = frame_state.lighting_scene->getLight(id);
        if (light == nullptr) {
          continue;
        }
        const Mat4 world = frame_state.lighting_scene->getWorldMatrix(id);
        m_spot_vp[i] = makeSpotViewProjection(Vec3(world[3]), lightWorldEmit(world),
                                              light->outer_cone_degrees,
                                              light->range * world_per_metre);
      }
    }
  }
  m_camera = frame_state.camera_position;
  m_vsm_light_view = makeVsmLightView(-m_light_dir, m_camera);
  m_inv_view_projection =
      glm::inverse(frame_state.projection * frame_state.view);
  uploadCasters(gpu_draws, gpu_draw_count, frame_state.scene_static);
  if (m_vsm_enabled && isValid(m_local.directional)) {
    stampCameraPages();
    if (k_vsm_gpu_page_mark) {
      mergeGpuMarksFromStaging(m_record_frame);
    } else {
      m_gpu_mark_count = 0;
    }
    compactCpuFlags();
  } else {
    m_marked_count = 0;
  }
  logVsmFrame();
  logDrops();
}

void MeshShadowSystem::logDrops() {
  if (m_local.point_dropped > 0 && !m_logged_point_drop) {
    LOG_WARN(
        "[MeshShadowSystem] dropped {} shadowing Point lights (cap {})",
        m_local.point_dropped, k_max_point_shadow_maps);
    m_logged_point_drop = true;
  }
  if (m_local.spot_dropped > 0 && !m_logged_spot_drop) {
    LOG_WARN("[MeshShadowSystem] dropped {} shadowing Spot lights (cap {})",
             m_local.spot_dropped, k_max_spot_shadow_maps);
    m_logged_spot_drop = true;
  }
}

void MeshShadowSystem::fillLinkIds(const LightComponent* light,
                                   ShadowMeshUniformCpu& ubo) const {
  ubo.link_count_pad = glm::uvec4(0);
  std::memset(ubo.link_ids, 0, sizeof(ubo.link_ids));
  if (light == nullptr || light->linking.empty()) {
    return;
  }
  const uint32_t n = static_cast<uint32_t>(
      eastl::min<size_t>(light->linking.size(), 16u));
  ubo.link_count_pad.x = n;
  for (uint32_t i = 0; i < n; ++i) {
    ubo.link_ids[i] = light->linking[i];
  }
}

void MeshShadowSystem::recordMeshTarget(VkCommandBuffer cmd, VkFramebuffer fb,
                                        VkExtent2D extent,
                                        const ShadowMeshUniformCpu& ubo,
                                        uint32_t layer_override,
                                        bool layered_pipeline,
                                        uint32_t dispatch_mul) {
  if (m_mesh_pipeline == VK_NULL_HANDLE || fb == VK_NULL_HANDLE ||
      m_meshlet_count == 0 || m_mesh_sets.empty() ||
      m_context->cmdDrawMeshTasksEXT() == nullptr) {
    return;
  }

  VkClearValue clear{};
  clear.depthStencil = {1.0f, 0};
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = m_depth_pass;
  begin.framebuffer = fb;
  begin.renderArea.extent = extent;
  begin.clearValueCount = 1;
  begin.pClearValues = &clear;
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
  VkViewport vp{};
  vp.width = static_cast<float>(extent.width);
  vp.height = static_cast<float>(extent.height);
  vp.maxDepth = 1.0f;
  VkRect2D scissor{};
  scissor.extent = extent;
  vkCmdSetViewport(cmd, 0, 1, &vp);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  VkPipeline pipe = (layered_pipeline && m_mesh_layered_pipeline != VK_NULL_HANDLE)
                        ? m_mesh_layered_pipeline
                        : m_mesh_pipeline;
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);

  const uint32_t set_base = m_record_frame * k_shadow_mesh_sets;
  const uint32_t n = eastl::min(
      m_instance_count,
      eastl::min(k_shadow_mesh_sets,
                 static_cast<uint32_t>(m_mesh_sets.size() > set_base
                                           ? m_mesh_sets.size() - set_base
                                           : 0u)));
  if (m_ubo_cursor >= k_shadow_ubo_slots) {
    if (!m_logged_ubo_overflow) {
      LOG_WARN("[MeshShadowSystem] shadow UBO slots exhausted");
      m_logged_ubo_overflow = true;
    }
    vkCmdEndRenderPass(cmd);
    return;
  }
  ShadowMeshUniformCpu page = ubo;
  page.layer = layer_override;
  const uint32_t slot = m_ubo_cursor++;
  uploadUboSlot(slot, &page, sizeof(page));
  const uint32_t dyn = slot * m_ubo_stride;
  struct ShadowMeshPushCpu {
    uint32_t instance_index;
    uint32_t pad0;
    uint32_t pad1;
    uint32_t pad2;
  };
  for (uint32_t i = 0; i < n; ++i) {
    const uint32_t meshlet_count = m_instance_cpu[i].meshlet_count;
    if (meshlet_count == 0 || m_mesh_sets[set_base + i] == VK_NULL_HANDLE) {
      continue;
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_mesh_pipe_layout,
                            0, 1, &m_mesh_sets[set_base + i], 1, &dyn);
    ShadowMeshPushCpu push{};
    push.instance_index = i;
    vkCmdPushConstants(cmd, m_mesh_pipe_layout, VK_SHADER_STAGE_TASK_BIT_EXT, 0,
                       sizeof(push), &push);
    const uint32_t groups = meshlet_count * eastl::max(1u, dispatch_mul);
    m_context->cmdDrawMeshTasksEXT()(cmd, groups, 1, 1);
  }
  vkCmdEndRenderPass(cmd);
}

void MeshShadowSystem::recordVsTarget(VkCommandBuffer cmd, VkFramebuffer fb,
                                      VkExtent2D extent, const glm::mat4& view_proj,
                                      const ForwardOpaqueDraw* opaque_draws,
                                      uint32_t opaque_draw_count,
                                      uint32_t) {
  if (m_vs_pipeline == nullptr || fb == VK_NULL_HANDLE ||
      m_vs_set == VK_NULL_HANDLE) {
    return;
  }
  struct VsUbo {
    glm::mat4 model;
    glm::mat4 light_view;
    glm::mat4 light_projection;
  };
  VkClearValue clear{};
  clear.depthStencil = {1.0f, 0};
  VkRenderPassBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin.renderPass = m_depth_pass;
  begin.framebuffer = fb;
  begin.renderArea.extent = extent;
  begin.clearValueCount = 1;
  begin.pClearValues = &clear;
  vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
  VkViewport vp{};
  vp.width = static_cast<float>(extent.width);
  vp.height = static_cast<float>(extent.height);
  vp.maxDepth = 1.0f;
  VkRect2D scissor{};
  scissor.extent = extent;
  vkCmdSetViewport(cmd, 0, 1, &vp);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_vs_pipeline->getGraphicsPipeline());
  for (const MeshShadowCasterDraw& caster : m_casters) {
    if (caster.draw == nullptr || caster.draw->gpu_mesh == nullptr) {
      continue;
    }
    GpuMesh* mesh = caster.draw->gpu_mesh;
    if (mesh->getVertexBuffer() == nullptr || mesh->getIndexBuffer() == nullptr) {
      continue;
    }
    VsUbo ubo{};
    ubo.model = caster.draw->model;
    ubo.light_view = glm::mat4(1.0f);
    ubo.light_projection = view_proj;
    uint32_t slot = 0;
    if (m_ubo_cursor < k_shadow_ubo_slots) {
      slot = m_ubo_cursor++;
    }
    uploadUboSlot(slot, &ubo, sizeof(ubo));
    VkDescriptorBufferInfo ubo_info{};
    ubo_info.buffer = m_shadow_ubo->getBuffer();
    ubo_info.offset = static_cast<VkDeviceSize>(slot) * m_ubo_stride;
    ubo_info.range = sizeof(VsUbo);
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_vs_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &ubo_info;
    vkUpdateDescriptorSets(m_context->getDevice(), 1, &write, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_vs_pipeline->getPipelineLayout(), 0, 1, &m_vs_set, 0,
                            nullptr);
    VkBuffer vb = mesh->getVertexBuffer()->getBuffer();
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
    vkCmdBindIndexBuffer(cmd, mesh->getIndexBuffer()->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT32);
    (void)opaque_draws;
    (void)opaque_draw_count;
    vkCmdDrawIndexed(cmd, mesh->getIndexCount(), 1, 0, 0, 0);
  }
  vkCmdEndRenderPass(cmd);
}

void MeshShadowSystem::recordPageMark(VkCommandBuffer cmd, VkImageView depth_view,
                                      VkImage depth_image, uint32_t width,
                                      uint32_t height,
                                      const glm::mat4& inv_view_projection,
                                      uint32_t frame_index) {
  if (!k_vsm_gpu_page_mark) {
    return;
  }
  const uint32_t frame = frame_index % k_frames;
  if (!m_vsm_enabled || m_mark_pipeline == VK_NULL_HANDLE ||
      depth_view == VK_NULL_HANDLE || m_mark_sets[frame] == VK_NULL_HANDLE ||
      m_mark_ubos[frame] == nullptr || m_page_flags_gpu[frame] == nullptr) {
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

  struct MarkUbo {
    glm::mat4 inv_view_projection;
    glm::mat4 light_view;
    glm::vec4 origin_levels;
    glm::vec4 vsm_params;
    glm::vec4 depth_size;
  } ubo{};
  ubo.inv_view_projection = inv_view_projection;
  ubo.light_view = m_vsm_light_view;
  ubo.origin_levels = glm::vec4(m_camera, static_cast<float>(k_vsm_first_level));
  ubo.vsm_params = glm::vec4(static_cast<float>(k_vsm_last_level),
                             static_cast<float>(k_vsm_pages_per_axis), 0.0f, 1.0f);
  ubo.depth_size = glm::vec4(static_cast<float>(width), static_cast<float>(height),
                             0.0f, 0.0f);
  m_mark_ubos[frame]->upload(&ubo, sizeof(ubo));

  vkCmdFillBuffer(cmd, m_page_flags_gpu[frame]->getBuffer(), 0,
                  vsmPageTableBytes(), 0);
  VkBufferMemoryBarrier fill_ready{};
  fill_ready.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  fill_ready.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  fill_ready.dstAccessMask =
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  fill_ready.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  fill_ready.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  fill_ready.buffer = m_page_flags_gpu[frame]->getBuffer();
  fill_ready.size = VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1,
                       &fill_ready, 0, nullptr);

  VkDescriptorBufferInfo ubo_info{};
  ubo_info.buffer = m_mark_ubos[frame]->getBuffer();
  ubo_info.range = sizeof(ubo);
  VkDescriptorImageInfo depth{};
  depth.imageView = depth_view;
  depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkDescriptorBufferInfo flags{};
  flags.buffer = m_page_flags_gpu[frame]->getBuffer();
  flags.range = VK_WHOLE_SIZE;
  VkWriteDescriptorSet writes[3]{};
  for (auto& w : writes) {
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = m_mark_sets[frame];
    w.descriptorCount = 1;
  }
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &ubo_info;
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo = &depth;
  writes[2].dstBinding = 2;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[2].pBufferInfo = &flags;
  vkUpdateDescriptorSets(m_context->getDevice(), 3, writes, 0, nullptr);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_mark_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_mark_pipe_layout, 0,
                          1, &m_mark_sets[frame], 0, nullptr);
  vkCmdDispatch(cmd, (width + 7u) / 8u, (height + 7u) / 8u, 1);

  VkBufferMemoryBarrier flags_done = fill_ready;
  flags_done.srcAccessMask =
      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
  flags_done.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1,
                       &flags_done, 0, nullptr);
  if (m_page_flags_staging[frame] != nullptr) {
    VkBufferCopy copy{};
    copy.size = vsmPageTableBytes();
    vkCmdCopyBuffer(cmd, m_page_flags_gpu[frame]->getBuffer(),
                    m_page_flags_staging[frame]->getBuffer(), 1, &copy);
    VkBufferMemoryBarrier to_host{};
    to_host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    to_host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    to_host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_host.buffer = m_page_flags_staging[frame]->getBuffer();
    to_host.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &to_host,
                         0, nullptr);
  }
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
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                             VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &depth_release);
  }
}

void MeshShadowSystem::recordFill(VkCommandBuffer cmd,
                                  const ForwardFrameState& frame_state,
                                  const ForwardOpaqueDraw* opaque_draws,
                                  uint32_t opaque_draw_count, uint32_t frame_index) {
  if (!frame_state.shadows_enabled) {
    return;
  }
  m_ubo_cursor = 0;
  const SceneInstance* scene = frame_state.lighting_scene;
  auto light_of = [&](EntityId id) -> const LightComponent* {
    return scene != nullptr ? scene->getLight(id) : nullptr;
  };

  if (m_vsm_enabled && isValid(m_local.directional) && m_marked_count > 0) {
    ShadowMeshUniformCpu ubo{};
    fillLinkIds(light_of(m_local.directional), ubo);
    for (uint32_t i = 0; i < m_marked_count; ++i) {
      if (i >= m_vsm.layer_fbs.size()) {
        break;
      }
      const uint32_t virt = m_physical_to_virtual[i];
      if (virt == k_vsm_unmarked_page) {
        continue;
      }
      const uint32_t pages_per_level = k_vsm_pages_per_level;
      const uint32_t local = virt % pages_per_level;
      const uint32_t level = k_vsm_first_level + virt / pages_per_level;
      const uint32_t page_x = local % k_vsm_pages_per_axis;
      const uint32_t page_y = local / k_vsm_pages_per_axis;
      ubo.view_projection =
          vsmPageViewProjection(m_vsm_light_view, level, page_x, page_y);
      recordMeshTarget(cmd, m_vsm.layer_fbs[i],
                       VkExtent2D{k_vsm_page_texels, k_vsm_page_texels}, ubo, i,
                       false, 1);
    }
  }

  const bool use_mesh = m_mesh_shaders && m_mesh_pipeline != VK_NULL_HANDLE;
  const bool layered = use_mesh && m_shader_output_layer &&
                       m_mesh_layered_pipeline != VK_NULL_HANDLE;
  // Cube far plane is the metre-authored range measured in world units.
  const float world_per_metre =
      scene != nullptr ? sceneWorldUnitsPerMetre(*scene) : 1.0f;
  for (uint32_t p = 0; p < m_local.point_count; ++p) {
    const LightComponent* light = light_of(m_local.points[p]);
    glm::vec3 pos(0.0f);
    float range = 10.0f * world_per_metre;
    if (scene != nullptr) {
      pos = Vec3(scene->getWorldMatrix(m_local.points[p])[3]);
    }
    if (light != nullptr) {
      range = light->range * world_per_metre;
    }
    ShadowMeshUniformCpu ubo{};
    fillLinkIds(light, ubo);
    ubo.light_position_range = glm::vec4(pos, range);
    const bool one_pass =
        layered && p < m_points.slot_fbs.size() &&
        m_points.slot_fbs[p] != VK_NULL_HANDLE;
    if (one_pass) {
      ubo.mode = 3;
      for (uint32_t face = 0; face < 6; ++face) {
        ubo.cube_face_vp[face] =
            makePointCubeFaceViewProjection(pos, face, range);
      }
      recordMeshTarget(cmd, m_points.slot_fbs[p],
                       VkExtent2D{k_local_shadow_map_size, k_local_shadow_map_size},
                       ubo, 0, true, 6);
    } else {
      ubo.mode = 1;
      for (uint32_t face = 0; face < 6; ++face) {
        const uint32_t layer = p * 6u + face;
        if (layer >= m_points.layer_fbs.size()) {
          break;
        }
        ubo.view_projection = makePointCubeFaceViewProjection(pos, face, range);
        if (use_mesh) {
          recordMeshTarget(cmd, m_points.layer_fbs[layer],
                           VkExtent2D{k_local_shadow_map_size, k_local_shadow_map_size},
                           ubo, layer, false, 1);
        } else {
          recordVsTarget(cmd, m_points.layer_fbs[layer],
                         VkExtent2D{k_local_shadow_map_size, k_local_shadow_map_size},
                         ubo.view_projection, opaque_draws, opaque_draw_count,
                         frame_index);
        }
      }
    }
  }

  for (uint32_t s = 0; s < m_local.spot_count; ++s) {
    if (s >= m_spots.layer_fbs.size()) {
      break;
    }
    ShadowMeshUniformCpu ubo{};
    fillLinkIds(light_of(m_local.spots[s]), ubo);
    ubo.view_projection = m_spot_vp[s];
    ubo.mode = 2;
    if (use_mesh) {
      recordMeshTarget(cmd, m_spots.layer_fbs[s],
                       VkExtent2D{k_local_shadow_map_size, k_local_shadow_map_size},
                       ubo, s, false, 1);
    } else {
      recordVsTarget(cmd, m_spots.layer_fbs[s],
                     VkExtent2D{k_local_shadow_map_size, k_local_shadow_map_size},
                     ubo.view_projection, opaque_draws, opaque_draw_count,
                     frame_index);
    }
  }

  if (m_vsm_enabled && m_marked_count > 0) {
    barrierDepthArrayToShaderRead(cmd, m_vsm, m_marked_count);
  }
  if (m_local.point_count > 0) {
    barrierDepthArrayToShaderRead(cmd, m_points, m_local.point_count * 6u);
  }
  if (m_local.spot_count > 0) {
    barrierDepthArrayToShaderRead(cmd, m_spots, m_local.spot_count);
  }
}

void MeshShadowSystem::writeSamplingDescriptors(VkDevice device, VkDescriptorSet set,
                                                uint32_t first_binding) const {
  if (set == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
      m_page_table == nullptr) {
    return;
  }
  VkDescriptorImageInfo pages{};
  pages.imageView = m_vsm.array_view != VK_NULL_HANDLE ? m_vsm.array_view
                                                       : m_dummy.array_view;
  pages.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkDescriptorBufferInfo table{};
  table.buffer = m_page_table->getBuffer();
  table.range = VK_WHOLE_SIZE;
  VkDescriptorImageInfo cubes{};
  cubes.imageView = m_points.cube_view != VK_NULL_HANDLE ? m_points.cube_view
                                                         : m_dummy.array_view;
  cubes.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo spots{};
  spots.imageView = m_spots.array_view != VK_NULL_HANDLE ? m_spots.array_view
                                                         : m_dummy.array_view;
  spots.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkWriteDescriptorSet writes[4]{};
  for (uint32_t i = 0; i < 4; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = set;
    writes[i].dstBinding = first_binding + i;
    writes[i].descriptorCount = 1;
  }
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[0].pImageInfo = &pages;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[1].pBufferInfo = &table;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[2].pImageInfo = &cubes;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[3].pImageInfo = &spots;
  vkUpdateDescriptorSets(device, 4, writes, 0, nullptr);
}

void MeshShadowSystem::applySamplingUniforms(ShadowSamplingUniform& sampling) const {
  sampling.vsm_origin_levels =
      glm::vec4(m_camera, static_cast<float>(k_vsm_first_level));
  sampling.vsm_params =
      glm::vec4(static_cast<float>(k_vsm_last_level),
                static_cast<float>(k_vsm_pages_per_axis),
                1.0f / static_cast<float>(k_vsm_page_texels),
                m_vsm_enabled && isValid(m_local.directional) ? 1.0f : 0.0f);
  sampling.vsm_light_view = m_vsm_light_view;
  for (uint32_t i = 0; i < k_max_spot_shadow_maps; ++i) {
    sampling.spot_view_projections[i] = m_spot_vp[i];
  }
}

}  // namespace Blunder
