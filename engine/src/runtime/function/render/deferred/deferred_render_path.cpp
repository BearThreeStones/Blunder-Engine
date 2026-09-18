#include "runtime/function/render/clustered/froxel_grid.h"
#include "runtime/function/render/deferred/deferred_render_path.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/geometric.hpp>
#include <vulkan/vulkan.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_opaque_draw.h"
#include "runtime/function/render/gpu_driven/gpu_driven_renderer.h"
#include "runtime/function/render/gpu_driven/gpu_driven_types.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/rhi/rhi_desc.h"

#include "EASTL/vector.h"
#include "runtime/function/render/shadow/mesh_shadow_system.h"
#include "runtime/function/render/shadow/shadow_map_target.h"
#include "runtime/function/render/slang/shader_resource_layout.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/viewport_style.h"
#include "runtime/function/render/vulkan/bindless_texture_table.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"
#include "runtime/function/scene/gpu_skinning.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

namespace {

constexpr uint32_t kDeferredDescriptorFrames = VulkanSync::k_max_frames_in_flight;

uint64_t hashMixU64(uint64_t hash, uint64_t value) {
  hash ^= value;
  hash *= 1099511628211ull;
  return hash;
}

uint64_t hashFloatBitsU64(uint64_t hash, float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return hashMixU64(hash, bits);
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
  if (buffer == VK_NULL_HANDLE) {
    return;
  }
  VkBufferMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.srcAccessMask = src;
  barrier.dstAccessMask = dst;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer = buffer;
  barrier.offset = 0;
  barrier.size = VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 1, &barrier, 0,
                       nullptr);
}

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
      vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr,
                               &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[DeferredRenderPath] vkCreateComputePipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}

uint32_t gbufferDescriptorIndex(uint32_t slot_index, uint32_t frame_index) {
  return slot_index * kDeferredDescriptorFrames + frame_index;
}

void bindViewportScissor(VkCommandBuffer cmd, uint32_t width, uint32_t height) {
  VkViewport viewport{};
  viewport.width = static_cast<float>(width);
  viewport.height = static_cast<float>(height);
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = {width, height};
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void fillGpuSkinPalette(const eastl::vector<glm::mat4>& joint_matrices,
                        GpuSkinPaletteData& out_palette) {
  for (uint32_t i = 0; i < k_max_gpu_skin_joints; ++i) {
    out_palette.joint_matrices[i] = glm::mat4(1.0f);
  }
  const uint32_t copy_count = static_cast<uint32_t>(
      std::min(joint_matrices.size(), static_cast<size_t>(k_max_gpu_skin_joints)));
  for (uint32_t i = 0; i < copy_count; ++i) {
    out_palette.joint_matrices[i] = joint_matrices[i];
  }
}

VkFormat pickDeferredReceiverFormat(VkPhysicalDevice physical_device) {
  const VkFormat candidates[] = {VK_FORMAT_R16_UINT, VK_FORMAT_R32_UINT};
  const VkFormatFeatureFlags needed =
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  for (VkFormat format : candidates) {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);
    if ((props.optimalTilingFeatures & needed) == needed) {
      return format;
    }
  }
  LOG_FATAL(
      "[DeferredRenderPath] no UINT receiver format with color attachment + "
      "sampled image");
  return VK_FORMAT_R32_UINT;
}

void createColorImage(VulkanContext* context, VulkanAllocator* allocator,
                      uint32_t width, uint32_t height, VkFormat format,
                      const char* label, VkImage* out_image,
                      VmaAllocation* out_allocation, VkImageView* out_view) {
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent = {width, height, 1};
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = format;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo alloc_info{};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VkResult result = vmaCreateImage(allocator->getAllocator(), &image_info,
                                   &alloc_info, out_image, out_allocation,
                                   nullptr);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[DeferredRenderPath] {} vmaCreateImage failed: {}", label,
              static_cast<int>(result));
  }

  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = *out_image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.layerCount = 1;
  result = vkCreateImageView(context->getDevice(), &view_info, nullptr, out_view);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[DeferredRenderPath] {} vkCreateImageView failed: {}", label,
              static_cast<int>(result));
  }
}

}  // namespace

DeferredRenderPath::~DeferredRenderPath() { shutdown(); }

void DeferredRenderPath::initialize(const DeferredRenderPathInit& init) {
  ASSERT(init.vk_context);
  ASSERT(init.vk_allocator);
  ASSERT(init.slang_compiler);
  ASSERT(init.offscreen);
  ASSERT(init.forward_path);

  m_vk_context = init.vk_context;
  m_vk_allocator = init.vk_allocator;
  m_slang_compiler = init.slang_compiler;
  m_offscreen = init.offscreen;
  m_forward_path = init.forward_path;
  m_shadow_map = init.shadow_map;
  m_fallback_texture = init.fallback_texture;
  m_mesh_shadows = init.mesh_shadows;
  m_receiver_format = pickDeferredReceiverFormat(m_vk_context->getPhysicalDevice());

  createRenderPasses();
  createPipelines();
  createDescriptorResources();

  const VkExtent2D extent = m_offscreen->getExtent();
  resize(extent.width, extent.height);

  LOG_INFO(
      "[DeferredRenderPath] ready: G-buffer {} planes (RGBA8 albedo+AO, RGBA8 "
      "oct-normal+metal+rough, UINT receiver format {}), light list cap {}, "
      "receiver slots {}",
      k_gbuffer_plane_count, static_cast<int>(m_receiver_format),
      static_cast<uint32_t>(k_max_deferred_light_list), k_max_receiver_slots);
}

void DeferredRenderPath::shutdown() {
  if (m_vk_context == nullptr) {
    return;
  }

  destroySlots();
  destroyDescriptorResources();
  destroyPipelines();
  destroyRenderPasses();

  m_fallback_texture = nullptr;
  m_shadow_map = nullptr;
  m_mesh_shadows = nullptr;
  m_forward_path = nullptr;
  m_offscreen = nullptr;
  m_slang_compiler = nullptr;
  m_vk_allocator = nullptr;
  m_vk_context = nullptr;
  m_width = 0;
  m_height = 0;
}

void DeferredRenderPath::createRenderPasses() {
  VkDevice device = m_vk_context->getDevice();

  {
    const VkFormat plane_formats[k_gbuffer_plane_count] = {
        k_albedo_ao_format, k_normal_metal_rough_format, m_receiver_format};

    VkAttachmentDescription attachments[k_gbuffer_plane_count + 1]{};
    VkAttachmentReference color_refs[k_gbuffer_plane_count]{};
    for (uint32_t i = 0; i < k_gbuffer_plane_count; ++i) {
      attachments[i].format = plane_formats[i];
      attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
      attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachments[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      color_refs[i].attachment = i;
      color_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    // Shared viewport offscreen depth: CLEAR here (this is the first depth
    // write of the frame); lighting and the LOAD overlay pass read it after.
    VkAttachmentDescription& depth = attachments[k_gbuffer_plane_count];
    depth.format = VK_FORMAT_D32_SFLOAT;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = k_gbuffer_plane_count;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = k_gbuffer_plane_count;
    subpass.pColorAttachments = color_refs;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = k_gbuffer_plane_count + 1;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 2;
    rp_info.pDependencies = dependencies;

    const VkResult result =
        vkCreateRenderPass(device, &rp_info, nullptr, &m_gbuffer_render_pass);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[DeferredRenderPath] G-buffer vkCreateRenderPass failed: {}",
                static_cast<int>(result));
    }
  }

  {
    // Lighting writes the viewport offscreen color (CLEAR; the fullscreen
    // triangle covers every pixel). No depth attachment: depth is sampled.
    VkAttachmentDescription color{};
    color.format = m_offscreen->getFormat();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_TRANSFER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.attachmentCount = 1;
    rp_info.pAttachments = &color;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 2;
    rp_info.pDependencies = dependencies;

    const VkResult result =
        vkCreateRenderPass(device, &rp_info, nullptr, &m_lighting_render_pass);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[DeferredRenderPath] lighting vkCreateRenderPass failed: {}",
                static_cast<int>(result));
    }
  }
}

void DeferredRenderPath::destroyRenderPasses() {
  if (m_vk_context == nullptr) {
    return;
  }
  VkDevice device = m_vk_context->getDevice();
  if (m_lighting_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_lighting_render_pass, nullptr);
    m_lighting_render_pass = VK_NULL_HANDLE;
  }
  if (m_gbuffer_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device, m_gbuffer_render_pass, nullptr);
    m_gbuffer_render_pass = VK_NULL_HANDLE;
  }
}

void DeferredRenderPath::createPipelines() {
  ASSERT(m_gbuffer_render_pass != VK_NULL_HANDLE);
  ASSERT(m_lighting_render_pass != VK_NULL_HANDLE);

  // Exact-match Shader resource layout: VulkanPipeline LOG_FATALs when the
  // extracted bindings differ from these record-path expectations.
  rhi::GraphicsPipelineDesc gbuffer_desc{};
  gbuffer_desc.shader_path = "engine/shaders/gbuffer.slang";
  gbuffer_desc.enable_vertex_input = true;
  gbuffer_desc.cull_mode = rhi::CullMode::None;
  gbuffer_desc.enable_depth_test = true;
  gbuffer_desc.enable_depth_write = true;
  gbuffer_desc.color_attachment_count = k_gbuffer_plane_count;
  fillGBufferExpectedBindings(gbuffer_desc.expected_descriptor_bindings,
                              gbuffer_desc.expected_descriptor_sets,
                              &gbuffer_desc.expected_descriptor_binding_count,
                              false, gbuffer_desc.expected_descriptor_kinds);
  m_gbuffer_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_gbuffer_pipeline->bind(m_vk_context, m_slang_compiler);
  m_gbuffer_pipeline->initializeWithRenderPass(m_gbuffer_render_pass,
                                               gbuffer_desc);

  rhi::GraphicsPipelineDesc skinned_desc = gbuffer_desc;
  skinned_desc.shader_path = "engine/shaders/gbuffer_skinned.slang";
  skinned_desc.enable_skinned_vertex_input = true;
  fillGBufferExpectedBindings(skinned_desc.expected_descriptor_bindings,
                              skinned_desc.expected_descriptor_sets,
                              &skinned_desc.expected_descriptor_binding_count,
                              true, skinned_desc.expected_descriptor_kinds);
  m_skinned_gbuffer_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_skinned_gbuffer_pipeline->bind(m_vk_context, m_slang_compiler);
  m_skinned_gbuffer_pipeline->initializeWithRenderPass(m_gbuffer_render_pass,
                                                       skinned_desc);

  rhi::GraphicsPipelineDesc lighting_desc{};
  lighting_desc.shader_path = "engine/shaders/deferred_lighting.slang";
  lighting_desc.enable_vertex_input = false;
  lighting_desc.cull_mode = rhi::CullMode::None;
  lighting_desc.enable_depth_test = false;
  lighting_desc.enable_depth_write = false;
  lighting_desc.color_attachment_count = 1;
  fillDeferredLightingExpectedBindings(
      lighting_desc.expected_descriptor_bindings,
      lighting_desc.expected_descriptor_sets,
      &lighting_desc.expected_descriptor_binding_count,
      lighting_desc.expected_descriptor_kinds);
  m_lighting_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_lighting_pipeline->bind(m_vk_context, m_slang_compiler);
  m_lighting_pipeline->initializeWithRenderPass(m_lighting_render_pass,
                                                lighting_desc);
  if (m_lighting_pipeline->nativePipeline()->usesBindlessTextureTable()) {
    LOG_FATAL(
        "[DeferredRenderPath] deferred_lighting.slang must not bind the "
        "Bindless texture table");
  }
  createFroxelFillPipeline();
}

void DeferredRenderPath::destroyPipelines() {
  destroyFroxelFillPipeline();
  if (m_lighting_pipeline) {
    m_lighting_pipeline->shutdown();
    m_lighting_pipeline.reset();
  }
  if (m_skinned_gbuffer_pipeline) {
    m_skinned_gbuffer_pipeline->shutdown();
    m_skinned_gbuffer_pipeline.reset();
  }
  if (m_gbuffer_pipeline) {
    m_gbuffer_pipeline->shutdown();
    m_gbuffer_pipeline.reset();
  }
}

void DeferredRenderPath::createDescriptorResources() {
  VkDevice device = m_vk_context->getDevice();
  const uint32_t total_slot_sets = k_max_receiver_slots * kDeferredDescriptorFrames;

  m_gbuffer_uniform_buffers.resize(total_slot_sets);
  m_skinned_gbuffer_uniform_buffers.resize(total_slot_sets);
  m_skinned_bone_palette_buffers.resize(total_slot_sets);
  for (uint32_t i = 0; i < total_slot_sets; ++i) {
    m_gbuffer_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_gbuffer_uniform_buffers[i]->create(
        m_vk_allocator, sizeof(GBufferMeshUniformData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_skinned_gbuffer_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_skinned_gbuffer_uniform_buffers[i]->create(
        m_vk_allocator, sizeof(GBufferMeshUniformData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_skinned_bone_palette_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_skinned_bone_palette_buffers[i]->create(
        m_vk_allocator, sizeof(GpuSkinPaletteData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  }
  m_lighting_uniform_buffers.resize(kDeferredDescriptorFrames);
  m_receiver_mask_buffers.resize(kDeferredDescriptorFrames);
  m_froxel_fill_ubos.resize(kDeferredDescriptorFrames);
  m_clustered_light_buffers.resize(kDeferredDescriptorFrames);
  m_froxel_overflow_buffers.resize(kDeferredDescriptorFrames);
  m_froxel_overflow_readbacks.resize(kDeferredDescriptorFrames);
  m_clustered_mask_buffers.resize(kDeferredDescriptorFrames);
  for (uint32_t i = 0; i < kDeferredDescriptorFrames; ++i) {
    m_lighting_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_lighting_uniform_buffers[i]->create(
        m_vk_allocator, sizeof(DeferredLightingUniformData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_receiver_mask_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_receiver_mask_buffers[i]->create(
        m_vk_allocator,
        sizeof(uint32_t) * k_gpu_driven_receiver_slot_count,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_froxel_fill_ubos[i] = eastl::make_unique<VulkanBuffer>();
    m_froxel_fill_ubos[i]->create(
        m_vk_allocator, sizeof(FroxelFillUniformData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_clustered_light_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_clustered_light_buffers[i]->create(
        m_vk_allocator, sizeof(GpuSceneLight) * k_max_clustered_lights,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_froxel_overflow_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_froxel_overflow_buffers[i]->create(
        m_vk_allocator, sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    m_froxel_overflow_readbacks[i] = eastl::make_unique<VulkanBuffer>();
    m_froxel_overflow_readbacks[i]->create(
        m_vk_allocator, sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_TO_CPU);
    const uint32_t overflow_zero = 0;
    m_froxel_overflow_readbacks[i]->upload(&overflow_zero,
                                           sizeof(overflow_zero));
    m_clustered_mask_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_clustered_mask_buffers[i]->create(
        m_vk_allocator,
        sizeof(uint32_t) * k_gpu_driven_receiver_slot_count *
            k_clustered_mask_words,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  }
  recreateFroxelGridBuffers(std::max(1u, m_width), std::max(1u, m_height));

  // static: 1 UBO; skinned: 2 UBO; lighting: 1 UBO; fill: 1 UBO.
  // lighting images: 5 sampled + 1 sampler; lighting SSBO: 5; fill SSBO: 4.
  VkDescriptorPoolSize pool_sizes[4]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount =
      total_slot_sets * 3u + kDeferredDescriptorFrames * 2u;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  pool_sizes[1].descriptorCount = kDeferredDescriptorFrames * 8u;
  pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  pool_sizes[2].descriptorCount = kDeferredDescriptorFrames;
  pool_sizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_sizes[3].descriptorCount = kDeferredDescriptorFrames * 10u;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 4;
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = total_slot_sets * 2u + kDeferredDescriptorFrames * 2u;
  const VkResult pool_result =
      vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool);
  if (pool_result != VK_SUCCESS) {
    LOG_FATAL("[DeferredRenderPath] vkCreateDescriptorPool failed: {}",
              static_cast<int>(pool_result));
  }

  auto allocate_sets = [&](VkDescriptorSetLayout layout, uint32_t count,
                           VkDescriptorSet* out_sets, const char* label) {
    eastl::vector<VkDescriptorSetLayout> layouts(count, layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = count;
    alloc_info.pSetLayouts = layouts.data();
    const VkResult result =
        vkAllocateDescriptorSets(device, &alloc_info, out_sets);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[DeferredRenderPath] {} vkAllocateDescriptorSets failed: {}",
                label, static_cast<int>(result));
    }
  };

  m_gbuffer_descriptor_sets.resize(total_slot_sets);
  allocate_sets(m_gbuffer_pipeline->nativePipeline()->getDescriptorSetLayout(),
                total_slot_sets, m_gbuffer_descriptor_sets.data(), "G-buffer");
  m_skinned_gbuffer_descriptor_sets.resize(total_slot_sets);
  allocate_sets(
      m_skinned_gbuffer_pipeline->nativePipeline()->getDescriptorSetLayout(),
      total_slot_sets, m_skinned_gbuffer_descriptor_sets.data(),
      "skinned G-buffer");
  allocate_sets(m_lighting_pipeline->nativePipeline()->getDescriptorSetLayout(),
                kDeferredDescriptorFrames, m_lighting_descriptor_sets.data(),
                "lighting");
  allocate_sets(m_froxel_fill_set_layout, kDeferredDescriptorFrames,
                m_froxel_fill_descriptor_sets.data(), "froxel fill");

  for (uint32_t i = 0; i < total_slot_sets; ++i) {
    VkDescriptorBufferInfo mesh_info{};
    mesh_info.buffer = m_gbuffer_uniform_buffers[i]->getBuffer();
    mesh_info.range = sizeof(GBufferMeshUniformData);

    VkDescriptorBufferInfo skinned_mesh_info{};
    skinned_mesh_info.buffer = m_skinned_gbuffer_uniform_buffers[i]->getBuffer();
    skinned_mesh_info.range = sizeof(GBufferMeshUniformData);

    VkDescriptorBufferInfo bone_info{};
    bone_info.buffer = m_skinned_bone_palette_buffers[i]->getBuffer();
    bone_info.range = sizeof(GpuSkinPaletteData);

    VkWriteDescriptorSet writes[3]{};
    for (VkWriteDescriptorSet& write : writes) {
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      write.descriptorCount = 1;
    }
    writes[0].dstSet = m_gbuffer_descriptor_sets[i];
    writes[0].dstBinding = 0;
    writes[0].pBufferInfo = &mesh_info;
    writes[1].dstSet = m_skinned_gbuffer_descriptor_sets[i];
    writes[1].dstBinding = 0;
    writes[1].pBufferInfo = &skinned_mesh_info;
    writes[2].dstSet = m_skinned_gbuffer_descriptor_sets[i];
    writes[2].dstBinding = 1;
    writes[2].pBufferInfo = &bone_info;
    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
  }

  for (uint32_t i = 0; i < kDeferredDescriptorFrames; ++i) {
    writeFroxelDescriptors(i);
  }
}

void DeferredRenderPath::destroyDescriptorResources() {
  if (m_vk_context == nullptr) {
    return;
  }
  VkDevice device = m_vk_context->getDevice();

  m_gbuffer_descriptor_sets.clear();
  m_skinned_gbuffer_descriptor_sets.clear();
  for (VkDescriptorSet& set : m_lighting_descriptor_sets) {
    set = VK_NULL_HANDLE;
  }
  for (VkDescriptorSet& set : m_froxel_fill_descriptor_sets) {
    set = VK_NULL_HANDLE;
  }
  if (m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
  }

  auto destroy_buffers = [](eastl::vector<eastl::unique_ptr<VulkanBuffer>>& list) {
    for (eastl::unique_ptr<VulkanBuffer>& buf : list) {
      if (buf) {
        buf->destroy();
        buf.reset();
      }
    }
    list.clear();
  };
  destroy_buffers(m_gbuffer_uniform_buffers);
  destroy_buffers(m_skinned_gbuffer_uniform_buffers);
  destroy_buffers(m_skinned_bone_palette_buffers);
  destroy_buffers(m_lighting_uniform_buffers);
  destroy_buffers(m_receiver_mask_buffers);
  destroy_buffers(m_froxel_fill_ubos);
  destroy_buffers(m_clustered_light_buffers);
  destroy_buffers(m_froxel_index_buffers);
  destroy_buffers(m_froxel_count_buffers);
  destroy_buffers(m_froxel_overflow_buffers);
  destroy_buffers(m_froxel_overflow_readbacks);
  destroy_buffers(m_clustered_mask_buffers);
  m_lighting_desc_slot.fill(~0u);
  m_froxel_desc_written.fill(0);
}

void DeferredRenderPath::resize(uint32_t width, uint32_t height) {
  if (m_vk_context == nullptr || width == 0 || height == 0) {
    return;
  }
  destroySlots();
  m_width = width;
  m_height = height;
  recreateFroxelGridBuffers(width, height);
  for (uint32_t frame = 0; frame < kDeferredDescriptorFrames; ++frame) {
    writeFroxelDescriptors(frame);
  }
  for (uint32_t slot = 0; slot < OffscreenRenderTarget::k_buffer_count; ++slot) {
    createSlot(slot);
  }
}

void DeferredRenderPath::dropGpuTargets() { destroySlots(); }

void DeferredRenderPath::createSlot(uint32_t slot_index) {
  ASSERT(slot_index < OffscreenRenderTarget::k_buffer_count);
  ASSERT(m_gbuffer_render_pass != VK_NULL_HANDLE);
  ASSERT(m_lighting_render_pass != VK_NULL_HANDLE);
  ASSERT(m_width > 0 && m_height > 0);

  GBufferSlot& slot = m_slots[slot_index];
  VkDevice device = m_vk_context->getDevice();

  const VkFormat plane_formats[k_gbuffer_plane_count] = {
      k_albedo_ao_format, k_normal_metal_rough_format, m_receiver_format};
  const char* plane_labels[k_gbuffer_plane_count] = {
      "albedo+AO", "oct-normal+metal+rough", "receiver"};
  for (uint32_t i = 0; i < k_gbuffer_plane_count; ++i) {
    createColorImage(m_vk_context, m_vk_allocator, m_width, m_height,
                     plane_formats[i], plane_labels[i], &slot.images[i],
                     &slot.allocations[i], &slot.views[i]);
  }

  VkImageView gbuffer_attachments[k_gbuffer_plane_count + 1] = {
      slot.views[0], slot.views[1], slot.views[2],
      m_offscreen->getDepthImageView(slot_index)};
  VkFramebufferCreateInfo gbuffer_fb{};
  gbuffer_fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  gbuffer_fb.renderPass = m_gbuffer_render_pass;
  gbuffer_fb.attachmentCount = k_gbuffer_plane_count + 1;
  gbuffer_fb.pAttachments = gbuffer_attachments;
  gbuffer_fb.width = m_width;
  gbuffer_fb.height = m_height;
  gbuffer_fb.layers = 1;
  VkResult result = vkCreateFramebuffer(device, &gbuffer_fb, nullptr,
                                        &slot.gbuffer_framebuffer);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[DeferredRenderPath] G-buffer vkCreateFramebuffer failed (slot {}): {}",
        slot_index, static_cast<int>(result));
  }

  VkImageView lighting_attachment = m_offscreen->getImageView(slot_index);
  VkFramebufferCreateInfo lighting_fb{};
  lighting_fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  lighting_fb.renderPass = m_lighting_render_pass;
  lighting_fb.attachmentCount = 1;
  lighting_fb.pAttachments = &lighting_attachment;
  lighting_fb.width = m_width;
  lighting_fb.height = m_height;
  lighting_fb.layers = 1;
  result = vkCreateFramebuffer(device, &lighting_fb, nullptr,
                               &slot.lighting_framebuffer);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[DeferredRenderPath] lighting vkCreateFramebuffer failed (slot {}): {}",
        slot_index, static_cast<int>(result));
  }
}

void DeferredRenderPath::destroySlot(uint32_t slot_index) {
  ASSERT(slot_index < OffscreenRenderTarget::k_buffer_count);
  GBufferSlot& slot = m_slots[slot_index];
  VkDevice device = m_vk_context->getDevice();

  if (slot.lighting_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, slot.lighting_framebuffer, nullptr);
    slot.lighting_framebuffer = VK_NULL_HANDLE;
  }
  if (slot.gbuffer_framebuffer != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(device, slot.gbuffer_framebuffer, nullptr);
    slot.gbuffer_framebuffer = VK_NULL_HANDLE;
  }
  for (uint32_t i = 0; i < k_gbuffer_plane_count; ++i) {
    if (slot.views[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(device, slot.views[i], nullptr);
      slot.views[i] = VK_NULL_HANDLE;
    }
    if (slot.images[i] != VK_NULL_HANDLE) {
      vmaDestroyImage(m_vk_allocator->getAllocator(), slot.images[i],
                      slot.allocations[i]);
      slot.images[i] = VK_NULL_HANDLE;
      slot.allocations[i] = VK_NULL_HANDLE;
    }
  }
}

void DeferredRenderPath::destroySlots() {
  if (m_vk_context == nullptr) {
    return;
  }
  for (uint32_t slot = 0; slot < OffscreenRenderTarget::k_buffer_count; ++slot) {
    destroySlot(slot);
  }
  m_width = 0;
  m_height = 0;
  m_lighting_desc_slot.fill(~0u);
}

void DeferredRenderPath::drawGBufferList(VkCommandBuffer cmd,
                                         const ForwardFrameState& frame_state,
                                         const ForwardOpaqueDraw* opaque_draws,
                                         uint32_t opaque_draw_count,
                                         uint32_t frame_index) {
  if (opaque_draws == nullptr || opaque_draw_count == 0) {
    return;
  }

  BindlessTextureTable* table = &m_vk_context->bindlessTextureTable();
  auto bindlessIndex = [&](VulkanTexture* texture) -> uint32_t {
    if (texture == nullptr || texture == m_fallback_texture) {
      return BindlessTextureIndexTable::k_fallback_index;
    }
    return table->acquire(texture);
  };

  // Material rules (unauthored-chrome default, alpha mode, two-sided) come
  // from applyPbrToMeshUniforms so both Render Paths agree. A studio copy of
  // the frame state keeps that call from gathering lights per draw; the
  // Deferred light list is uploaded once by the lighting pass instead.
  ForwardFrameState material_state = frame_state;
  material_state.live_scene_lighting = false;
  material_state.lighting_scene = nullptr;

  vulkan_backend::VulkanGraphicsPipeline* last_pipeline = nullptr;
  uint32_t drawn = 0;
  uint32_t skipped = 0;
  for (uint32_t draw_i = 0; draw_i < opaque_draw_count; ++draw_i) {
    const ForwardOpaqueDraw& draw = opaque_draws[draw_i];
    if (draw.vertex_buffer == nullptr || draw.index_buffer == nullptr ||
        draw.index_count == 0 || draw.slot_index >= k_max_receiver_slots) {
      ++skipped;
      continue;
    }

    const bool gpu_skinned = !draw.gpu_bone_palette.empty();
    vulkan_backend::VulkanGraphicsPipeline* active_pipeline =
        gpu_skinned ? m_skinned_gbuffer_pipeline.get() : m_gbuffer_pipeline.get();
    if (active_pipeline == nullptr) {
      ++skipped;
      continue;
    }
    VulkanPipeline* native = active_pipeline->nativePipeline();

    if (active_pipeline != last_pipeline) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        native->getGraphicsPipeline());
      if (native->usesBindlessTextureTable()) {
        const VkDescriptorSet table_set = table->descriptorSet();
        if (table_set != VK_NULL_HANDLE) {
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  native->getPipelineLayout(), 1, 1, &table_set,
                                  0, nullptr);
        }
      }
      last_pipeline = active_pipeline;
    }

    const uint32_t descriptor_index =
        gbufferDescriptorIndex(draw.slot_index, frame_index);
    if (descriptor_index >= m_gbuffer_descriptor_sets.size()) {
      ++skipped;
      continue;
    }

    ForwardMeshUniformData material{};
    applyPbrToMeshUniforms(material, draw.material, frame_state.shading,
                           material_state, draw.alpha_mode, draw.alpha_cutoff,
                           draw.double_sided, draw.entity_id);

    GBufferMeshUniformData mesh_ubo{};
    mesh_ubo.model = draw.model;
    mesh_ubo.view = frame_state.view;
    mesh_ubo.projection = frame_state.projection;
    mesh_ubo.normal_matrix = draw.normal_matrix;
    mesh_ubo.base_color_factor = material.base_color_factor;
    mesh_ubo.material_flags = material.material_flags;
    mesh_ubo.metallic_roughness_factors = material.metallic_roughness_factors;
    mesh_ubo.pbr_texture_flags = material.pbr_texture_flags;
    mesh_ubo.bindless_texture_indices = glm::uvec4(
        bindlessIndex(draw.base_color_texture),
        bindlessIndex(draw.metallic_roughness_texture),
        bindlessIndex(draw.normal_texture),
        bindlessIndex(draw.occlusion_texture));
    // Overflow returns index 0; do not treat slot-0 pixels as extra PBR maps.
    mesh_ubo.pbr_texture_flags.x =
        mesh_ubo.bindless_texture_indices.y != 0 ? 1.0f : 0.0f;
    mesh_ubo.pbr_texture_flags.y =
        mesh_ubo.bindless_texture_indices.z != 0 ? 1.0f : 0.0f;
    mesh_ubo.pbr_texture_flags.z =
        mesh_ubo.bindless_texture_indices.w != 0 ? 1.0f : 0.0f;
    mesh_ubo.receiver = glm::uvec4(draw.slot_index, 0u, 0u, 0u);

    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    if (gpu_skinned) {
      GpuSkinPaletteData palette{};
      fillGpuSkinPalette(draw.gpu_bone_palette, palette);
      m_skinned_bone_palette_buffers[descriptor_index]->upload(&palette,
                                                               sizeof(palette));
      m_skinned_gbuffer_uniform_buffers[descriptor_index]->upload(
          &mesh_ubo, sizeof(mesh_ubo));
      descriptor_set = m_skinned_gbuffer_descriptor_sets[descriptor_index];
    } else {
      m_gbuffer_uniform_buffers[descriptor_index]->upload(&mesh_ubo,
                                                          sizeof(mesh_ubo));
      descriptor_set = m_gbuffer_descriptor_sets[descriptor_index];
    }

    VkBuffer vertex_buffers[] = {draw.vertex_buffer->getBuffer()};
    VkDeviceSize vertex_offsets[] = {0};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            native->getPipelineLayout(), 0, 1, &descriptor_set,
                            0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, vertex_offsets);
    vkCmdBindIndexBuffer(cmd, draw.index_buffer->getBuffer(), 0,
                         VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, draw.index_count, 1, 0, 0, 0);
    ++drawn;
  }
  {
    static int s_gb = 0;
    if (s_gb < 4) {
      ++s_gb;
      if (FILE* dump = std::fopen(
              "E:/cursor/stores/bc-a87e1603-397e-40c2-ac5d-d4373b287a4a/"
              "internal/gizmo-cam.log",
              "a")) {
        const glm::vec4 clip =
            frame_state.projection * frame_state.view *
            glm::vec4(-60.0f, 40.0f, 650.0f, 1.0f);
        std::fprintf(dump,
                     "gbuffer n=%d count=%u drawn=%u skipped=%u idx0=%u "
                     "clip.w=%.2f ndc=(%.3f,%.3f,%.3f) cam=(%.1f,%.1f,%.1f)\n",
                     s_gb, opaque_draw_count, drawn, skipped,
                     opaque_draws[0].index_count, clip.w,
                     clip.x / std::max(clip.w, 1e-6f),
                     clip.y / std::max(clip.w, 1e-6f),
                     clip.z / std::max(clip.w, 1e-6f),
                     frame_state.camera_position.x, frame_state.camera_position.y,
                     frame_state.camera_position.z);
        std::fclose(dump);
      }
    }
  }
}

void DeferredRenderPath::uploadLightingUniforms(
    const ForwardFrameState& frame_state, const ForwardOpaqueDraw* opaque_draws,
    uint32_t opaque_draw_count, uint32_t frame_index,
    GpuDrivenRenderer* gpu_driven) {
  DeferredLightingUniformData ubo{};
  ubo.inv_view_projection =
      glm::inverse(frame_state.projection * frame_state.view);
  ubo.light_view_projection = frame_state.light_view_projection;
  ubo.camera_position = glm::vec4(frame_state.camera_position, 1.0f);
  const float inv_shadow_map_size =
      1.0f / static_cast<float>(ShadowMapTarget::k_shadow_map_size);
  ubo.shadow_params = glm::vec4(
      frame_state.shadow_bias, frame_state.shadows_enabled ? 1.0f : 0.0f,
      inv_shadow_map_size, 0.0f);
  if (frame_state.mesh_shadows != nullptr) {
    frame_state.mesh_shadows->applySamplingUniforms(ubo.shadow_sampling);
  }
  // Live lighting has no IBL. Keep a floor so a clipped directional map
  // cannot zero albedo (SE-world forest is ~175 m; courtyard far was 60).
  ubo.ambient_color = glm::vec4(k_live_lighting_ambient_floor);
  ubo.background_color = viewportBackgroundColor();
  ubo.view = frame_state.view;

  const FroxelGridDim dim = makeFroxelGridDim(std::max(1u, m_width),
                                              std::max(1u, m_height));
  m_froxel_dim = dim;
  const glm::vec4 froxel_screen(
      static_cast<float>(dim.width_px), static_cast<float>(dim.height_px),
      static_cast<float>(dim.tiles_x), static_cast<float>(dim.tiles_y));
  const SceneInstance* scene =
      frame_state.live_scene_lighting ? frame_state.lighting_scene : nullptr;
  float lighting_far = frame_state.far_clip;
  if (scene != nullptr && scene->hasWorldBounds()) {
    const AABB& bounds = scene->getWorldBounds();
    lighting_far = clusteredLightingFar(frame_state.far_clip,
                                        glm::length(bounds.max - bounds.min),
                                        frame_state.camera_distance);
  }
  const glm::vec4 froxel_z(
      frame_state.near_clip, lighting_far,
      static_cast<float>(dim.slices), static_cast<float>(dim.tile_size_px));
  ubo.froxel_screen = froxel_screen;
  ubo.froxel_z = froxel_z;

  EvaluatedLight list[k_max_deferred_light_list];
  size_t list_count = 0;
  if (scene != nullptr) {
    list_count = buildDeferredFullscreenLightList(
        *scene, list, k_max_deferred_light_list);
  }
  ubo.light_count = glm::vec4(static_cast<float>(list_count), 0.0f, 0.0f, 0.0f);
  for (size_t i = 0; i < list_count; ++i) {
    packEvaluatedLight(ubo.lights[i], list[i], frame_state.shadow_caster_id,
                       frame_state.shadows_enabled, &frame_state.local_shadows);
  }

  EvaluatedLight clustered[k_max_clustered_lights];
  size_t clustered_count = 0;
  if (scene != nullptr) {
    clustered_count = buildClusteredPointSpotList(*scene, clustered,
                                                  k_max_clustered_lights);
  }
  m_uploaded_clustered_light_count = static_cast<uint32_t>(clustered_count);
  ubo.clustered_params = glm::vec4(
      static_cast<float>(clustered_count),
      frame_state.shading.froxel_occupancy_heatmap ? 1.0f : 0.0f, 0.0f, 0.0f);

  GpuSceneLight clustered_gpu[k_max_clustered_lights]{};
  for (size_t i = 0; i < clustered_count; ++i) {
    packEvaluatedLight(clustered_gpu[i], clustered[i],
                       frame_state.shadow_caster_id, frame_state.shadows_enabled,
                       &frame_state.local_shadows);
  }

  FroxelFillUniformData fill{};
  fill.view = frame_state.view;
  fill.inv_projection = glm::inverse(frame_state.projection);
  fill.screen = froxel_screen;
  fill.z_params = froxel_z;
  fill.light_count = glm::vec4(static_cast<float>(clustered_count), 0.0f, 0.0f,
                               0.0f);

  bool rebuild_masks = m_receiver_mask_cpu.size() != k_gpu_driven_receiver_slot_count;
  uint64_t mask_fp = m_lighting_mask_fingerprint;
  if (!frame_state.scene_static || mask_fp == 0 || rebuild_masks) {
    mask_fp = 14695981039346656037ull;
    mask_fp = hashMixU64(mask_fp, opaque_draw_count);
    if (opaque_draws != nullptr) {
      for (uint32_t draw_i = 0; draw_i < opaque_draw_count; ++draw_i) {
        mask_fp = hashMixU64(mask_fp, static_cast<uint32_t>(opaque_draws[draw_i].entity_id));
        mask_fp = hashMixU64(mask_fp, opaque_draws[draw_i].slot_index);
      }
    }
    if (gpu_driven != nullptr) {
      const eastl::vector<GpuDrivenDraw>& packed = gpu_driven->packedDraws();
      mask_fp = hashMixU64(mask_fp, packed.size());
      for (const GpuDrivenDraw& draw : packed) {
        mask_fp = hashMixU64(mask_fp, static_cast<uint32_t>(draw.entity_id));
        mask_fp = hashMixU64(mask_fp, draw.receiver_id);
      }
    }
    mask_fp = hashMixU64(mask_fp, clustered_count);
    for (size_t i = 0; i < clustered_count; ++i) {
      mask_fp = hashMixU64(mask_fp, static_cast<uint32_t>(clustered[i].entity_id));
      mask_fp = hashFloatBitsU64(mask_fp, clustered[i].color_times_intensity.x);
      mask_fp = hashFloatBitsU64(mask_fp, clustered[i].color_times_intensity.y);
      mask_fp = hashFloatBitsU64(mask_fp, clustered[i].color_times_intensity.z);
      mask_fp = hashFloatBitsU64(mask_fp, clustered[i].range);
    }
    mask_fp = hashMixU64(mask_fp, list_count);
    for (size_t i = 0; i < list_count; ++i) {
      mask_fp = hashMixU64(mask_fp, static_cast<uint32_t>(list[i].entity_id));
    }
    rebuild_masks = mask_fp != m_lighting_mask_fingerprint || rebuild_masks;
  } else {
    rebuild_masks = false;
  }
  if (rebuild_masks) {
    m_receiver_mask_cpu.assign(k_gpu_driven_receiver_slot_count, 0u);
    m_clustered_mask_cpu.assign(
        static_cast<size_t>(k_gpu_driven_receiver_slot_count) * k_clustered_mask_words,
        0u);

    auto fill_ubo_mask = [&](uint32_t slot, EntityId entity_id) {
      if (scene == nullptr || list_count == 0 ||
          slot >= k_gpu_driven_receiver_slot_count) {
        return;
      }
      EvaluatedLight per_receiver[k_max_evaluated_lights_per_mesh];
      const size_t affecting = evaluateLightsForReceiver(
          *scene, entity_id, list, list_count, per_receiver,
          k_max_evaluated_lights_per_mesh);
      uint32_t mask = 0;
      for (size_t j = 0; j < affecting; ++j) {
        for (size_t k = 0; k < list_count; ++k) {
          if (list[k].entity_id == per_receiver[j].entity_id) {
            mask |= 1u << static_cast<uint32_t>(k);
            break;
          }
        }
      }
      m_receiver_mask_cpu[slot] = mask;
    };
    auto fill_clustered_mask = [&](uint32_t slot, EntityId entity_id) {
      if (scene == nullptr || clustered_count == 0 ||
          slot >= k_gpu_driven_receiver_slot_count) {
        return;
      }
      EvaluatedLight per_receiver[k_max_clustered_lights];
      const size_t affecting = evaluateLightsForReceiver(
          *scene, entity_id, clustered, clustered_count, per_receiver,
          k_max_clustered_lights);
      uint32_t* words =
          m_clustered_mask_cpu.data() +
          static_cast<size_t>(slot) * k_clustered_mask_words;
      for (size_t j = 0; j < affecting; ++j) {
        for (size_t k = 0; k < clustered_count; ++k) {
          if (clustered[k].entity_id == per_receiver[j].entity_id) {
            words[k >> 5] |= 1u << (static_cast<uint32_t>(k) & 31u);
            break;
          }
        }
      }
    };
    auto fill_masks = [&](uint32_t slot, EntityId entity_id) {
      fill_ubo_mask(slot, entity_id);
      fill_clustered_mask(slot, entity_id);
    };

    if (opaque_draws != nullptr) {
      for (uint32_t draw_i = 0; draw_i < opaque_draw_count; ++draw_i) {
        const ForwardOpaqueDraw& draw = opaque_draws[draw_i];
        fill_masks(draw.slot_index, draw.entity_id);
      }
    }
    if (gpu_driven != nullptr) {
      const eastl::vector<GpuDrivenDraw>& packed = gpu_driven->packedDraws();
      for (const GpuDrivenDraw& draw : packed) {
        fill_masks(draw.receiver_id, draw.entity_id);
      }
    }
    std::memcpy(m_cached_clustered_gpu, clustered_gpu, sizeof(clustered_gpu));
    m_lighting_mask_fingerprint = mask_fp;
  }

  m_lighting_uniform_buffers[frame_index]->upload(&ubo, sizeof(ubo));
  if (m_uploaded_lighting_mask_fingerprint[frame_index] != m_lighting_mask_fingerprint) {
    m_receiver_mask_buffers[frame_index]->upload(
        m_receiver_mask_cpu.data(), sizeof(uint32_t) * k_gpu_driven_receiver_slot_count);
    m_clustered_light_buffers[frame_index]->upload(
        m_cached_clustered_gpu, sizeof(GpuSceneLight) * k_max_clustered_lights);
    m_clustered_mask_buffers[frame_index]->upload(
        m_clustered_mask_cpu.data(),
        sizeof(uint32_t) * k_gpu_driven_receiver_slot_count *
            k_clustered_mask_words);
    m_uploaded_lighting_mask_fingerprint[frame_index] = m_lighting_mask_fingerprint;
  }
  m_froxel_fill_ubos[frame_index]->upload(&fill, sizeof(fill));
}

void DeferredRenderPath::writeLightingDescriptors(uint32_t frame_index,
                                                  uint32_t slot_index) {
  if (frame_index < m_lighting_desc_slot.size() &&
      m_lighting_desc_slot[frame_index] == slot_index) {
    return;
  }
  VkDevice device = m_vk_context->getDevice();
  const GBufferSlot& slot = m_slots[slot_index];

  VkDescriptorImageInfo plane_infos[k_gbuffer_plane_count]{};
  for (uint32_t i = 0; i < k_gbuffer_plane_count; ++i) {
    plane_infos[i].imageView = slot.views[i];
    plane_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }
  VkDescriptorImageInfo depth_info{};
  depth_info.imageView = m_offscreen->getDepthImageView(slot_index);
  depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkDescriptorImageInfo shadow_image_info{};
  VkDescriptorImageInfo shadow_sampler_info{};
  if (m_shadow_map != nullptr) {
    shadow_image_info.imageView = m_shadow_map->getDepthImageView();
    shadow_image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    shadow_sampler_info.sampler = m_shadow_map->getComparisonSampler();
  } else if (m_fallback_texture != nullptr) {
    shadow_image_info.imageView = m_fallback_texture->getImageView();
    shadow_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shadow_sampler_info.sampler = m_fallback_texture->getSampler();
  } else {
    LOG_FATAL(
        "[DeferredRenderPath] Shader resource layout requires shadow bindings "
        "5 and 6, but neither a ShadowMapTarget nor a fallback texture is "
        "available");
  }

  VkWriteDescriptorSet writes[6]{};
  for (uint32_t i = 0; i < 6; ++i) {
    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].dstSet = m_lighting_descriptor_sets[frame_index];
    writes[i].descriptorCount = 1;
    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  }
  writes[0].dstBinding = 1;
  writes[0].pImageInfo = &plane_infos[0];
  writes[1].dstBinding = 2;
  writes[1].pImageInfo = &plane_infos[1];
  writes[2].dstBinding = 3;
  writes[2].pImageInfo = &plane_infos[2];
  writes[3].dstBinding = 4;
  writes[3].pImageInfo = &depth_info;
  writes[4].dstBinding = 5;
  writes[4].pImageInfo = &shadow_image_info;
  writes[5].dstBinding = 6;
  writes[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[5].pImageInfo = &shadow_sampler_info;
  vkUpdateDescriptorSets(device, 6, writes, 0, nullptr);
  if (m_mesh_shadows != nullptr) {
    m_mesh_shadows->writeSamplingDescriptors(
        device, m_lighting_descriptor_sets[frame_index], 12);
  }
  if (frame_index < m_lighting_desc_slot.size()) {
    m_lighting_desc_slot[frame_index] = slot_index;
  }
}

void DeferredRenderPath::writeFroxelDescriptors(uint32_t frame_index) {
  if (frame_index >= kDeferredDescriptorFrames || m_vk_context == nullptr) {
    return;
  }
  if (frame_index < m_froxel_desc_written.size() &&
      m_froxel_desc_written[frame_index] != 0) {
    return;
  }
  VkDevice device = m_vk_context->getDevice();

  if (m_lighting_descriptor_sets[frame_index] != VK_NULL_HANDLE &&
      m_lighting_uniform_buffers[frame_index] &&
      m_receiver_mask_buffers[frame_index] &&
      m_clustered_light_buffers[frame_index] &&
      m_froxel_index_buffers.size() > frame_index &&
      m_froxel_index_buffers[frame_index] &&
      m_froxel_count_buffers.size() > frame_index &&
      m_froxel_count_buffers[frame_index] &&
      m_clustered_mask_buffers[frame_index]) {
    VkDescriptorBufferInfo ubo{};
    ubo.buffer = m_lighting_uniform_buffers[frame_index]->getBuffer();
    ubo.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo masks{};
    masks.buffer = m_receiver_mask_buffers[frame_index]->getBuffer();
    masks.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo clustered{};
    clustered.buffer = m_clustered_light_buffers[frame_index]->getBuffer();
    clustered.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo indices{};
    indices.buffer = m_froxel_index_buffers[frame_index]->getBuffer();
    indices.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo counts{};
    counts.buffer = m_froxel_count_buffers[frame_index]->getBuffer();
    counts.range = VK_WHOLE_SIZE;
    VkDescriptorBufferInfo clustered_masks{};
    clustered_masks.buffer = m_clustered_mask_buffers[frame_index]->getBuffer();
    clustered_masks.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[6]{};
    for (VkWriteDescriptorSet& write : writes) {
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = m_lighting_descriptor_sets[frame_index];
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &ubo;
    writes[1].dstBinding = 7;
    writes[1].pBufferInfo = &masks;
    writes[2].dstBinding = 8;
    writes[2].pBufferInfo = &clustered;
    writes[3].dstBinding = 9;
    writes[3].pBufferInfo = &indices;
    writes[4].dstBinding = 10;
    writes[4].pBufferInfo = &counts;
    writes[5].dstBinding = 11;
    writes[5].pBufferInfo = &clustered_masks;
    vkUpdateDescriptorSets(device, 6, writes, 0, nullptr);
  }

  if (m_froxel_fill_descriptor_sets[frame_index] == VK_NULL_HANDLE ||
      !m_froxel_fill_ubos[frame_index] || !m_clustered_light_buffers[frame_index] ||
      m_froxel_index_buffers.size() <= frame_index ||
      !m_froxel_index_buffers[frame_index] ||
      m_froxel_count_buffers.size() <= frame_index ||
      !m_froxel_count_buffers[frame_index] ||
      !m_froxel_overflow_buffers[frame_index]) {
    return;
  }

  VkDescriptorBufferInfo fill_ubo{};
  fill_ubo.buffer = m_froxel_fill_ubos[frame_index]->getBuffer();
  fill_ubo.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo fill_clustered{};
  fill_clustered.buffer = m_clustered_light_buffers[frame_index]->getBuffer();
  fill_clustered.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo fill_indices{};
  fill_indices.buffer = m_froxel_index_buffers[frame_index]->getBuffer();
  fill_indices.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo fill_counts{};
  fill_counts.buffer = m_froxel_count_buffers[frame_index]->getBuffer();
  fill_counts.range = VK_WHOLE_SIZE;
  VkDescriptorBufferInfo fill_overflow{};
  fill_overflow.buffer = m_froxel_overflow_buffers[frame_index]->getBuffer();
  fill_overflow.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet fill_writes[5]{};
  for (VkWriteDescriptorSet& write : fill_writes) {
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_froxel_fill_descriptor_sets[frame_index];
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  }
  fill_writes[0].dstBinding = 0;
  fill_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  fill_writes[0].pBufferInfo = &fill_ubo;
  fill_writes[1].dstBinding = 1;
  fill_writes[1].pBufferInfo = &fill_clustered;
  fill_writes[2].dstBinding = 2;
  fill_writes[2].pBufferInfo = &fill_indices;
  fill_writes[3].dstBinding = 3;
  fill_writes[3].pBufferInfo = &fill_counts;
  fill_writes[4].dstBinding = 4;
  fill_writes[4].pBufferInfo = &fill_overflow;
  vkUpdateDescriptorSets(device, 5, fill_writes, 0, nullptr);
  if (frame_index < m_froxel_desc_written.size()) {
    m_froxel_desc_written[frame_index] = 1;
  }
}

void DeferredRenderPath::createFroxelFillPipeline() {
  ASSERT(m_vk_context);
  ASSERT(m_slang_compiler);
  uint32_t bindings[k_max_expected_descriptor_bindings];
  uint32_t sets[k_max_expected_descriptor_bindings];
  ShaderDescriptorKind kinds[k_max_expected_descriptor_bindings]{};
  uint32_t binding_count = 0;
  fillFroxelFillExpectedBindings(bindings, sets, &binding_count, kinds);

  const SlangCompiler::ComputeProgramResult program =
      m_slang_compiler->compileComputeProgram("engine/shaders/froxel_fill.slang",
                                              "main");
  if (!shaderResourceBindingsMatch(program.layout, bindings, binding_count, sets,
                                   kinds)) {
    LOG_FATAL(
        "[DeferredRenderPath] froxel_fill.slang resource layout does not match "
        "record-path bindings (extracted {}, expected {})",
        program.layout.count, binding_count);
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

  VkDevice device = m_vk_context->getDevice();
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
  layout_info.pBindings = layout_bindings.data();
  const VkResult layout_result = vkCreateDescriptorSetLayout(
      device, &layout_info, nullptr, &m_froxel_fill_set_layout);
  if (layout_result != VK_SUCCESS) {
    LOG_FATAL(
        "[DeferredRenderPath] froxel fill vkCreateDescriptorSetLayout failed: "
        "{}",
        static_cast<int>(layout_result));
  }

  VkPipelineLayoutCreateInfo pipe_layout_info{};
  pipe_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipe_layout_info.setLayoutCount = 1;
  pipe_layout_info.pSetLayouts = &m_froxel_fill_set_layout;
  const VkResult pipe_layout_result = vkCreatePipelineLayout(
      device, &pipe_layout_info, nullptr, &m_froxel_fill_pipe_layout);
  if (pipe_layout_result != VK_SUCCESS) {
    LOG_FATAL(
        "[DeferredRenderPath] froxel fill vkCreatePipelineLayout failed: {}",
        static_cast<int>(pipe_layout_result));
  }

  VkShaderModuleCreateInfo module_info{};
  module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_info.codeSize = program.compute.spirv_code.size();
  module_info.pCode =
      reinterpret_cast<const uint32_t*>(program.compute.spirv_code.data());
  VkShaderModule module = VK_NULL_HANDLE;
  const VkResult module_result =
      vkCreateShaderModule(device, &module_info, nullptr, &module);
  if (module_result != VK_SUCCESS) {
    LOG_FATAL("[DeferredRenderPath] froxel fill vkCreateShaderModule failed: {}",
              static_cast<int>(module_result));
  }
  m_froxel_fill_pipeline = createVkComputePipeline(
      device, m_froxel_fill_pipe_layout, module,
      program.compute.entry_point_name.c_str());
  vkDestroyShaderModule(device, module, nullptr);
}

void DeferredRenderPath::destroyFroxelFillPipeline() {
  if (m_vk_context == nullptr) {
    return;
  }
  VkDevice device = m_vk_context->getDevice();
  if (m_froxel_fill_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_froxel_fill_pipeline, nullptr);
    m_froxel_fill_pipeline = VK_NULL_HANDLE;
  }
  if (m_froxel_fill_pipe_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_froxel_fill_pipe_layout, nullptr);
    m_froxel_fill_pipe_layout = VK_NULL_HANDLE;
  }
  if (m_froxel_fill_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_froxel_fill_set_layout, nullptr);
    m_froxel_fill_set_layout = VK_NULL_HANDLE;
  }
}

void DeferredRenderPath::recreateFroxelGridBuffers(uint32_t width,
                                                   uint32_t height) {
  if (m_vk_allocator == nullptr) {
    return;
  }
  const FroxelGridDim dim = makeFroxelGridDim(width, height);
  const bool same_dim =
      dim.tiles_x == m_froxel_dim.tiles_x && dim.tiles_y == m_froxel_dim.tiles_y &&
      dim.slices == m_froxel_dim.slices &&
      dim.tile_size_px == m_froxel_dim.tile_size_px &&
      dim.width_px == m_froxel_dim.width_px &&
      dim.height_px == m_froxel_dim.height_px;
  if (same_dim && m_froxel_index_buffers.size() == kDeferredDescriptorFrames &&
      m_froxel_count_buffers.size() == kDeferredDescriptorFrames &&
      m_froxel_index_buffers[0] && m_froxel_count_buffers[0]) {
    return;
  }
  m_froxel_dim = dim;
  m_froxel_desc_written.fill(0);

  auto reset_list = [](eastl::vector<eastl::unique_ptr<VulkanBuffer>>& list) {
    for (eastl::unique_ptr<VulkanBuffer>& buf : list) {
      if (buf) {
        buf->destroy();
        buf.reset();
      }
    }
    list.clear();
    list.resize(kDeferredDescriptorFrames);
  };
  reset_list(m_froxel_index_buffers);
  reset_list(m_froxel_count_buffers);

  const VkDeviceSize index_bytes =
      sizeof(uint32_t) * std::max(1u, froxelIndexBufferCount(dim));
  const VkDeviceSize count_bytes =
      sizeof(uint32_t) * std::max(1u, froxelCount(dim));
  for (uint32_t i = 0; i < kDeferredDescriptorFrames; ++i) {
    m_froxel_index_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_froxel_index_buffers[i]->create(
        m_vk_allocator, index_bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    m_froxel_count_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_froxel_count_buffers[i]->create(
        m_vk_allocator, count_bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
  }
}

void DeferredRenderPath::recordFroxelFill(VkCommandBuffer cmd,
                                          const ForwardFrameState& frame_state,
                                          uint32_t frame_index) {
  if (cmd == VK_NULL_HANDLE || frame_index >= kDeferredDescriptorFrames ||
      m_froxel_fill_pipeline == VK_NULL_HANDLE ||
      m_froxel_fill_pipe_layout == VK_NULL_HANDLE ||
      m_froxel_fill_descriptor_sets[frame_index] == VK_NULL_HANDLE ||
      !m_froxel_count_buffers[frame_index] ||
      !m_froxel_overflow_buffers[frame_index] ||
      !m_froxel_overflow_readbacks[frame_index] ||
      !m_froxel_index_buffers[frame_index]) {
    return;
  }

  uint32_t dropped = 0;
  const bool read_overflow = frame_state.shading.froxel_occupancy_heatmap;
  if (read_overflow &&
      m_froxel_overflow_readbacks[frame_index]->download(&dropped,
                                                        sizeof(dropped))) {
    m_froxel_dropped_light_assignments = dropped;
    if (dropped > 0) {
      m_froxel_dropped_light_assignments_total += dropped;
      LOG_INFO(
          "[DeferredRenderPath] froxel overflow: dropped {} light assignments "
          "this frame (total {})",
          dropped, m_froxel_dropped_light_assignments_total);
    }
  }

  vkCmdFillBuffer(cmd, m_froxel_count_buffers[frame_index]->getBuffer(), 0,
                  m_froxel_count_buffers[frame_index]->getSize(), 0);
  vkCmdFillBuffer(cmd, m_froxel_overflow_buffers[frame_index]->getBuffer(), 0,
                  m_froxel_overflow_buffers[frame_index]->getSize(), 0);

  VkBufferMemoryBarrier fill_barriers[2]{};
  for (VkBufferMemoryBarrier& barrier : fill_barriers) {
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
  }
  fill_barriers[0].buffer = m_froxel_count_buffers[frame_index]->getBuffer();
  fill_barriers[1].buffer = m_froxel_overflow_buffers[frame_index]->getBuffer();
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 2,
                       fill_barriers, 0, nullptr);

  if (m_uploaded_clustered_light_count > 0) {
    const uint32_t cells = froxelCount(m_froxel_dim);
    const VkDeviceSize count_bytes =
        sizeof(uint32_t) * std::max(1u, cells);
    const VkDeviceSize index_bytes =
        sizeof(uint32_t) * std::max(1u, froxelIndexBufferCount(m_froxel_dim));
    if (m_froxel_count_buffers[frame_index]->getSize() < count_bytes ||
        m_froxel_index_buffers[frame_index]->getSize() < index_bytes) {
      LOG_ERROR(
          "[DeferredRenderPath] froxel buffers smaller than UBO grid "
          "({}x{}x{}); skipping fill",
          m_froxel_dim.tiles_x, m_froxel_dim.tiles_y, m_froxel_dim.slices);
    } else {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                        m_froxel_fill_pipeline);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              m_froxel_fill_pipe_layout, 0, 1,
                              &m_froxel_fill_descriptor_sets[frame_index], 0,
                              nullptr);
      const uint32_t groups_x = ceilDivU32(m_froxel_dim.tiles_x, 4u);
      const uint32_t groups_y = ceilDivU32(m_froxel_dim.tiles_y, 4u);
      const uint32_t groups_z = ceilDivU32(m_froxel_dim.slices, 4u);
      vkCmdDispatch(cmd, std::max(1u, groups_x), std::max(1u, groups_y),
                    std::max(1u, groups_z));
    }
  }

  cmdBufferBarrier(cmd, m_froxel_overflow_buffers[frame_index]->getBuffer(),
                   VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_TRANSFER_READ_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);
  if (read_overflow) {
    VkBufferCopy overflow_copy{};
    overflow_copy.size = sizeof(uint32_t);
    vkCmdCopyBuffer(cmd, m_froxel_overflow_buffers[frame_index]->getBuffer(),
                    m_froxel_overflow_readbacks[frame_index]->getBuffer(), 1,
                    &overflow_copy);
    cmdBufferBarrier(cmd, m_froxel_overflow_readbacks[frame_index]->getBuffer(),
                     VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
  }

  cmdBufferBarrier(cmd, m_froxel_index_buffers[frame_index]->getBuffer(),
                   VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  cmdBufferBarrier(cmd, m_froxel_count_buffers[frame_index]->getBuffer(),
                   VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                   VK_ACCESS_SHADER_READ_BIT,
                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                       VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void DeferredRenderPath::cmdBarrierForLoadPass(VkCommandBuffer cmd) {
  // Lighting wrote the offscreen color; G-buffer wrote depth and lighting read
  // it. The LOAD overlay pass expects SHADER_READ_ONLY /
  // DEPTH_STENCIL_READ_ONLY as initial layouts and reads both as attachments.
  VkImageMemoryBarrier barriers[2]{};
  barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].image = m_offscreen->getImage();
  barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barriers[0].subresourceRange.levelCount = 1;
  barriers[0].subresourceRange.layerCount = 1;
  barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barriers[0].dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  barriers[1] = barriers[0];
  barriers[1].image = m_offscreen->getDepthImage();
  barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barriers[1].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  barriers[1].srcAccessMask =
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
  barriers[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  vkCmdPipelineBarrier(cmd,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                       0, 0, nullptr, 0, nullptr, 2, barriers);
}

bool DeferredRenderPath::prepareViewportRecord(uint32_t frame_index,
                                               VkExtent2D* extent,
                                               uint32_t* slot_index) const {
  ASSERT(m_vk_context);
  ASSERT(m_offscreen);
  ASSERT(m_forward_path);
  ASSERT(extent);
  ASSERT(slot_index);

  const VkExtent2D current = m_offscreen->getExtent();
  if (current.width == 0 || current.height == 0) {
    return false;
  }
  if (current.width != m_width || current.height != m_height) {
    // Recreating both FIF G-buffer sets here would destroy in-flight
    // framebuffers. applyDeferredOffscreenResize waits idle, then dropGpuTargets
    // before OffscreenRenderTarget::resize.
    return false;
  }

  ASSERT(frame_index < kDeferredDescriptorFrames);
  const uint32_t slot = m_offscreen->getActiveBufferIndex();
  const GBufferSlot& slot_data = m_slots[slot];
  ASSERT(slot_data.gbuffer_framebuffer != VK_NULL_HANDLE);
  ASSERT(slot_data.lighting_framebuffer != VK_NULL_HANDLE);
  *extent = current;
  *slot_index = slot;
  return true;
}

void DeferredRenderPath::recordGBufferPass(
    VkCommandBuffer command_buffer, const ForwardFrameState& frame_state,
    const ForwardOpaqueDraw* opaque_draws, uint32_t opaque_draw_count,
    uint32_t frame_index, GpuDrivenRenderer* gpu_driven,
    const GpuDrivenDraw* gpu_draws, uint32_t gpu_draw_count) {
  VkExtent2D extent{};
  uint32_t slot_index = 0;
  if (!prepareViewportRecord(frame_index, &extent, &slot_index)) {
    if (g_runtime_global_context.m_render_system) {
      g_runtime_global_context.m_render_system->requestViewportRedraw();
    }
    return;
  }

  const GBufferSlot& slot = m_slots[slot_index];
  SecondaryCommandBufferPool& pool = m_vk_context->secondaryCommandBuffers();
  ASSERT(pool.isAllocated());
  constexpr SecondaryStream stream = SecondaryStream::viewport;

  const bool record_gpu = gpu_driven != nullptr && gpu_draws != nullptr &&
                          gpu_draw_count > 0;
  if (record_gpu) {
    gpu_driven->uploadAndCull(command_buffer, frame_index, gpu_draws,
                              gpu_draw_count, frame_state,
                              frame_state.camera_distance < 2000.0f);
  }

  m_forward_path->recordShadowPass(command_buffer, frame_state, opaque_draws,
                                   opaque_draw_count, frame_index, stream,
                                   frame_index,
                                   record_gpu ? gpu_driven : nullptr, gpu_draws,
                                   gpu_draw_count);

  {
    VkClearValue clears[k_gbuffer_plane_count + 1]{};
    clears[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clears[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clears[2].color.uint32[0] = k_receiver_no_geometry;
    clears[k_gbuffer_plane_count].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp_begin{};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = m_gbuffer_render_pass;
    rp_begin.framebuffer = slot.gbuffer_framebuffer;
    rp_begin.renderArea.extent = extent;
    rp_begin.clearValueCount = k_gbuffer_plane_count + 1;
    rp_begin.pClearValues = clears;
    vkCmdBeginRenderPass(command_buffer, &rp_begin,
                         VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    m_offscreen->setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    const VkCommandBuffer gbuffer_secondary =
        pool.begin(stream, SecondaryPass::gbuffer_opaque, frame_index,
                   m_gbuffer_render_pass, slot.gbuffer_framebuffer);
    bindViewportScissor(gbuffer_secondary, extent.width, extent.height);
    drawGBufferList(gbuffer_secondary, frame_state, opaque_draws,
                    opaque_draw_count, frame_index);
    if (record_gpu) {
      gpu_driven->recordGBufferIndirect(gbuffer_secondary, frame_index,
                                        frame_state, false, m_fallback_texture);
      if (gpu_driven->latePassEnabled()) {
        gpu_driven->recordGBufferIndirect(gbuffer_secondary, frame_index,
                                          frame_state, true, m_fallback_texture);
      }
    }
    pool.end(stream, SecondaryPass::gbuffer_opaque, frame_index);
    SecondaryCommandBufferPool::execute(command_buffer, gbuffer_secondary);
    vkCmdEndRenderPass(command_buffer);
    m_offscreen->setDepthLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
  }

  if (m_mesh_shadows != nullptr && m_mesh_shadows->vsmEnabled()) {
    m_mesh_shadows->recordPageMark(
        command_buffer, m_offscreen->getDepthImageView(slot_index),
        m_offscreen->getDepthImage(slot_index), extent.width, extent.height,
        glm::inverse(frame_state.projection * frame_state.view), frame_index);
  }

  if (record_gpu) {
    gpu_driven->recordBuildHiZ(command_buffer, frame_index,
                               m_offscreen->getDepthImageView(slot_index),
                               extent.width, extent.height,
                               m_offscreen->getDepthImage(slot_index));
  }
}

void DeferredRenderPath::recordLightingPass(
    VkCommandBuffer command_buffer, const ForwardFrameState& frame_state,
    const ForwardOpaqueDraw* opaque_draws, uint32_t opaque_draw_count,
    const ForwardOpaqueDraw* transparent_draws,
    uint32_t transparent_draw_count, uint32_t frame_index,
    GpuDrivenRenderer* gpu_driven) {
  VkExtent2D extent{};
  uint32_t slot_index = 0;
  if (!prepareViewportRecord(frame_index, &extent, &slot_index)) {
    if (g_runtime_global_context.m_render_system) {
      g_runtime_global_context.m_render_system->requestViewportRedraw();
    }
    return;
  }

  const GBufferSlot& slot = m_slots[slot_index];
  SecondaryCommandBufferPool& pool = m_vk_context->secondaryCommandBuffers();
  ASSERT(pool.isAllocated());
  constexpr SecondaryStream stream = SecondaryStream::viewport;

  // Lighting: fullscreen triangle into the offscreen color (CLEAR).
  {
    uploadLightingUniforms(frame_state, opaque_draws, opaque_draw_count,
                           frame_index, gpu_driven);
    writeLightingDescriptors(frame_index, slot_index);
    writeFroxelDescriptors(frame_index);
    recordFroxelFill(command_buffer, frame_state, frame_index);

    VkClearValue clear{};
    clear.color = {{kViewportBackgroundRgb, kViewportBackgroundRgb,
                    kViewportBackgroundRgb, 1.0f}};
    VkRenderPassBeginInfo rp_begin{};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = m_lighting_render_pass;
    rp_begin.framebuffer = slot.lighting_framebuffer;
    rp_begin.renderArea.extent = extent;
    rp_begin.clearValueCount = 1;
    rp_begin.pClearValues = &clear;
    vkCmdBeginRenderPass(command_buffer, &rp_begin,
                         VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
    m_offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    const VkCommandBuffer lighting_secondary =
        pool.begin(stream, SecondaryPass::deferred_lighting, frame_index,
                   m_lighting_render_pass, slot.lighting_framebuffer);
    VulkanPipeline* native = m_lighting_pipeline->nativePipeline();
    vkCmdBindPipeline(lighting_secondary, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      native->getGraphicsPipeline());
    vkCmdBindDescriptorSets(lighting_secondary, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            native->getPipelineLayout(), 0, 1,
                            &m_lighting_descriptor_sets[frame_index], 0,
                            nullptr);
    bindViewportScissor(lighting_secondary, extent.width, extent.height);
    vkCmdDraw(lighting_secondary, 3, 1, 0, 0);
    pool.end(stream, SecondaryPass::deferred_lighting, frame_index);
    SecondaryCommandBufferPool::execute(command_buffer, lighting_secondary);
    vkCmdEndRenderPass(command_buffer);
    m_offscreen->setCurrentLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  // Barriers on the PRIMARY, then LOAD color + depth for scene overlays and
  // blend-transparent Forward draws (same secondaries as ForwardRenderPath).
  cmdBarrierForLoadPass(command_buffer);
  m_offscreen->beginLoadRenderPass(command_buffer,
                                   VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
  m_forward_path->recordSceneOverlayAndTransparent(
      command_buffer, m_offscreen->getLoadRenderPass(),
      m_offscreen->getFramebuffer(), extent, frame_state, transparent_draws,
      transparent_draw_count, frame_index, /*draw_overlays=*/true, stream,
      frame_index);
  m_offscreen->endLoadRenderPass(command_buffer);
}

}  // namespace Blunder
