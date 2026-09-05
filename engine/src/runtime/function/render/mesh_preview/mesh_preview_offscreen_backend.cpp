#include "runtime/function/render/mesh_preview/mesh_preview_offscreen_backend.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cgltf.h>

#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_opaque_draw.h"
#include "runtime/function/render/forward/forward_render_path.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/mesh_preview/mesh_preview_draw_builder.h"
#include "runtime/function/render/mesh_preview/mesh_preview_frame_state.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/i_render_backend.h"
#include "runtime/function/render/rhi/i_render_device.h"
#include "runtime/function/render/slang/shader_resource_layout.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"
#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"
#include "runtime/function/render/vulkan_backend/vulkan_render_backend.h"
#include "runtime/function/scene/gpu_skinning.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {

namespace {

constexpr uint32_t k_fallback_texture_size = 4u;

eastl::vector<uint8_t> buildFallbackTexturePixels() {
  eastl::vector<uint8_t> pixels(static_cast<size_t>(k_fallback_texture_size) *
                                k_fallback_texture_size * 4u, 255u);
  for (size_t i = 0; i < pixels.size(); i += 4u) {
    pixels[i + 0] = 220u;
    pixels[i + 1] = 220u;
    pixels[i + 2] = 220u;
  }
  return pixels;
}

void appendForwardDraw(const MeshPreviewSubmeshDraw& submesh_draw, GpuMesh* gpu_mesh,
                       VulkanTexture* base_color_texture,
                       VulkanTexture* metallic_roughness_texture,
                       VulkanTexture* normal_texture,
                       VulkanTexture* occlusion_texture,
                       uint32_t slot_index,
                       eastl::vector<ForwardOpaqueDraw>& opaque_out,
                       eastl::vector<ForwardOpaqueDraw>& transparent_out) {
  if (gpu_mesh == nullptr || gpu_mesh->getVertexBuffer() == nullptr ||
      gpu_mesh->getIndexBuffer() == nullptr || gpu_mesh->getIndexCount() == 0) {
    return;
  }

  const MaterialAsset* material = submesh_draw.material.get();
  cgltf_alpha_mode alpha_mode = cgltf_alpha_mode_opaque;
  float alpha_cutoff = 0.5f;
  bool double_sided = false;

  if (material != nullptr) {
    alpha_mode = material->getAlphaMode();
    alpha_cutoff = material->getAlphaCutoff();
    double_sided = material->isDoubleSided();
  }

  ForwardOpaqueDraw draw{};
  draw.slot_index = slot_index;
  draw.vertex_buffer = gpu_mesh->getVertexBuffer();
  draw.index_buffer = gpu_mesh->getIndexBuffer();
  draw.index_count = gpu_mesh->getIndexCount();
  draw.model = submesh_draw.model;
  draw.normal_matrix = glm::transpose(glm::inverse(submesh_draw.model));
  draw.material = material;
  draw.base_color_texture = base_color_texture;
  draw.metallic_roughness_texture = metallic_roughness_texture;
  draw.normal_texture = normal_texture;
  draw.occlusion_texture = occlusion_texture;
  draw.alpha_cutoff = alpha_cutoff;
  draw.alpha_mode = alpha_mode;
  draw.double_sided = double_sided;
  draw.entity_id = submesh_draw.entity_id;

  if (submesh_draw.mesh && submesh_draw.mesh->isSkinned() &&
      shouldUseGpuSkinning(*submesh_draw.mesh)) {
    draw.gpu_bone_palette.resize(submesh_draw.mesh->getSkinData().joint_to_bone.size(),
                                 glm::mat4(1.0f));
  }

  if (material != nullptr && material->usesForwardTransparentPass()) {
    transparent_out.push_back(draw);
  } else {
    opaque_out.push_back(draw);
  }
}

}  // namespace

MeshPreviewOffscreenBackend::MeshPreviewOffscreenBackend() = default;

MeshPreviewOffscreenBackend::~MeshPreviewOffscreenBackend() { shutdown(); }

bool MeshPreviewOffscreenBackend::initialize(rhi::IRenderBackend* render_backend,
                                           AssetManager* asset_manager) {
  shutdown();
  if (render_backend == nullptr ||
      render_backend->type() != rhi::RenderBackendType::Vulkan) {
    return false;
  }
  m_render_backend = render_backend;
  m_asset_manager = asset_manager;
  return true;
}

void MeshPreviewOffscreenBackend::shutdownRenderPath() {
  if (m_forward_path) {
    m_forward_path->shutdown();
    m_forward_path.reset();
  }
  if (m_mesh_pipeline) {
    m_mesh_pipeline->shutdown();
    m_mesh_pipeline.reset();
  }
  if (m_transparent_pipeline) {
    m_transparent_pipeline->shutdown();
    m_transparent_pipeline.reset();
  }
  if (m_skinned_mesh_pipeline) {
    m_skinned_mesh_pipeline->shutdown();
    m_skinned_mesh_pipeline.reset();
  }
  if (m_skinned_transparent_pipeline) {
    m_skinned_transparent_pipeline->shutdown();
    m_skinned_transparent_pipeline.reset();
  }
  m_pipeline_width = 0;
  m_pipeline_height = 0;
}

void MeshPreviewOffscreenBackend::shutdown() {
  shutdownRenderPath();
  for (auto& entry : m_gpu_meshes) {
    if (entry.second) {
      entry.second->destroy();
    }
  }
  m_gpu_meshes.clear();
  for (auto& entry : m_uploaded_textures) {
    if (entry.second) {
      entry.second->destroy();
    }
  }
  m_uploaded_textures.clear();
  if (m_fallback_texture_owner) {
    m_fallback_texture_owner->destroy();
    m_fallback_texture_owner.reset();
  }
  m_fallback_texture = nullptr;
  if (m_readback_staging) {
    m_readback_staging->destroy();
    m_readback_staging.reset();
  }
  if (m_offscreen) {
    static_cast<vulkan_backend::VulkanOffscreenTarget*>(m_offscreen.get())
        ->shutdown();
    m_offscreen.reset();
  }
  m_render_backend = nullptr;
  m_asset_manager = nullptr;
  m_width = 0;
  m_height = 0;
  m_last_submitted_draw_count = 0;
}

bool MeshPreviewOffscreenBackend::ensureResources(uint32_t width, uint32_t height) {
  if (m_render_backend == nullptr || width == 0 || height == 0) {
    return false;
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanAllocator* allocator = backend->nativeAllocator();
  if (allocator == nullptr) {
    return false;
  }

  if (!m_offscreen) {
    rhi::OffscreenTargetDesc desc{};
    desc.width = width;
    desc.height = height;
    m_offscreen = backend->device().createOffscreenTarget(desc);
  } else if (m_width != width || m_height != height) {
    m_offscreen->resize(width, height);
  }

  if (!m_readback_staging || m_width != width || m_height != height) {
    if (m_readback_staging) {
      m_readback_staging->destroy();
    } else {
      m_readback_staging = eastl::make_unique<VulkanBuffer>();
    }
    const VkDeviceSize bytes =
        static_cast<VkDeviceSize>(width) * height * 4u;
    m_readback_staging->create(allocator, bytes,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VMA_MEMORY_USAGE_GPU_TO_CPU);
  }

  m_width = width;
  m_height = height;
  return m_offscreen != nullptr && m_readback_staging != nullptr &&
         ensureRenderPath(width, height);
}

bool MeshPreviewOffscreenBackend::ensureRenderPath(uint32_t width, uint32_t height) {
  if (m_render_backend == nullptr || m_offscreen == nullptr) {
    return false;
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanContext* context = backend->nativeVulkanContext();
  if (context == nullptr) {
    return false;
  }

  if (m_pipeline_width == width && m_pipeline_height == height &&
      m_forward_path && m_mesh_pipeline) {
    return true;
  }

  shutdownRenderPath();

  rhi::GraphicsPipelineDesc mesh_pipeline_desc{};
  mesh_pipeline_desc.shader_path = "engine/shaders/pbr.slang";
  mesh_pipeline_desc.enable_vertex_input = true;
  mesh_pipeline_desc.cull_mode = rhi::CullMode::None;
  mesh_pipeline_desc.enable_depth_test = true;
  mesh_pipeline_desc.enable_depth_write = true;
  fillPbrMeshExpectedBindings(mesh_pipeline_desc.expected_descriptor_bindings,
                              mesh_pipeline_desc.expected_descriptor_sets,
                              &mesh_pipeline_desc.expected_descriptor_binding_count,
                              false);
  m_mesh_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_mesh_pipeline->bind(context, backend->nativeSlangCompiler());
  m_mesh_pipeline->initialize(*m_offscreen, mesh_pipeline_desc);

  rhi::GraphicsPipelineDesc transparent_pipeline_desc = mesh_pipeline_desc;
  transparent_pipeline_desc.enable_blend = true;
  transparent_pipeline_desc.enable_depth_write = false;
  transparent_pipeline_desc.cull_mode = rhi::CullMode::None;
  transparent_pipeline_desc.shared_descriptor_set_layout = reinterpret_cast<uint64_t>(
      m_mesh_pipeline->nativePipeline()->getDescriptorSetLayout());
  m_transparent_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_transparent_pipeline->bind(context, backend->nativeSlangCompiler());
  m_transparent_pipeline->initialize(*m_offscreen, transparent_pipeline_desc);

  rhi::GraphicsPipelineDesc skinned_mesh_pipeline_desc = mesh_pipeline_desc;
  skinned_mesh_pipeline_desc.shader_path = "engine/shaders/pbr_skinned.slang";
  skinned_mesh_pipeline_desc.enable_skinned_vertex_input = true;
  fillPbrMeshExpectedBindings(
      skinned_mesh_pipeline_desc.expected_descriptor_bindings,
      skinned_mesh_pipeline_desc.expected_descriptor_sets,
      &skinned_mesh_pipeline_desc.expected_descriptor_binding_count, true);
  m_skinned_mesh_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_skinned_mesh_pipeline->bind(context, backend->nativeSlangCompiler());
  m_skinned_mesh_pipeline->initialize(*m_offscreen, skinned_mesh_pipeline_desc);

  rhi::GraphicsPipelineDesc skinned_transparent_pipeline_desc =
      skinned_mesh_pipeline_desc;
  skinned_transparent_pipeline_desc.enable_blend = true;
  skinned_transparent_pipeline_desc.enable_depth_write = false;
  skinned_transparent_pipeline_desc.shared_descriptor_set_layout =
      reinterpret_cast<uint64_t>(
          m_skinned_mesh_pipeline->nativePipeline()->getDescriptorSetLayout());
  m_skinned_transparent_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_skinned_transparent_pipeline->bind(context, backend->nativeSlangCompiler());
  m_skinned_transparent_pipeline->initialize(*m_offscreen,
                                             skinned_transparent_pipeline_desc);

  m_forward_path = eastl::make_unique<ForwardRenderPath>();
  ForwardRenderPathInit forward_init{};
  forward_init.vk_context = context;
  forward_init.vk_allocator = backend->nativeAllocator();
  forward_init.offscreen = m_offscreen.get();
  forward_init.opaque_pipeline = m_mesh_pipeline.get();
  forward_init.transparent_pipeline = m_transparent_pipeline.get();
  forward_init.skinned_opaque_pipeline = m_skinned_mesh_pipeline.get();
  forward_init.skinned_transparent_pipeline = m_skinned_transparent_pipeline.get();
  forward_init.fallback_texture = getFallbackTexture();
  m_forward_path->initialize(forward_init);

  m_pipeline_width = width;
  m_pipeline_height = height;
  return true;
}

VulkanTexture* MeshPreviewOffscreenBackend::getFallbackTexture() {
  if (m_fallback_texture != nullptr) {
    return m_fallback_texture;
  }
  if (m_render_backend == nullptr ||
      m_render_backend->type() != rhi::RenderBackendType::Vulkan) {
    return nullptr;
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanContext* context = backend->nativeVulkanContext();
  VulkanAllocator* allocator = backend->nativeAllocator();
  if (context == nullptr || allocator == nullptr) {
    return nullptr;
  }

  Asset::Meta meta;
  meta.virtual_path = "generated://mesh_preview/fallback";
  Texture2DAsset fallback_asset(meta, k_fallback_texture_size,
                                k_fallback_texture_size, 4u,
                                buildFallbackTexturePixels());
  m_fallback_texture_owner = eastl::make_unique<VulkanTexture>();
  m_fallback_texture_owner->createFromTexture2DAsset(context, allocator,
                                                     fallback_asset);
  m_fallback_texture = m_fallback_texture_owner.get();
  return m_fallback_texture;
}

VulkanTexture* MeshPreviewOffscreenBackend::ensureTextureUploaded(
    const Texture2DAsset* texture_asset) {
  if (texture_asset == nullptr || m_render_backend == nullptr ||
      m_render_backend->type() != rhi::RenderBackendType::Vulkan) {
    return nullptr;
  }

  eastl::string cache_key = texture_asset->getVirtualPath();
  if (cache_key.empty()) {
    const std::filesystem::path& absolute_path = texture_asset->getAbsolutePath();
    if (!absolute_path.empty()) {
      cache_key = eastl::string(absolute_path.generic_string().c_str());
    } else {
      cache_key = "generated://mesh_preview/anonymous_texture";
    }
  }

  if (auto it = m_uploaded_textures.find(cache_key);
      it != m_uploaded_textures.end()) {
    return it->second.get();
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanContext* context = backend->nativeVulkanContext();
  VulkanAllocator* allocator = backend->nativeAllocator();
  if (context == nullptr || allocator == nullptr) {
    return nullptr;
  }

  auto uploaded_texture = eastl::make_unique<VulkanTexture>();
  uploaded_texture->createFromTexture2DAsset(context, allocator, *texture_asset);
  VulkanTexture* uploaded_texture_ptr = uploaded_texture.get();
  m_uploaded_textures[cache_key] = eastl::move(uploaded_texture);
  return uploaded_texture_ptr;
}

GpuMesh* MeshPreviewOffscreenBackend::getOrUploadGpuMesh(
    const MeshAsset& mesh_asset, const eastl::string& cache_key) {
  if (m_render_backend == nullptr ||
      m_render_backend->type() != rhi::RenderBackendType::Vulkan) {
    return nullptr;
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanAllocator* allocator = backend->nativeAllocator();
  if (allocator == nullptr) {
    return nullptr;
  }

  if (auto it = m_gpu_meshes.find(cache_key); it != m_gpu_meshes.end()) {
    return it->second.get();
  }

  eastl::unique_ptr<GpuMesh> uploaded_mesh;
  if (mesh_asset.isSkinned() && shouldUseGpuSkinning(mesh_asset)) {
    eastl::vector<SkinnedMeshVertex> skinned_vertices;
    packSkinnedMeshVertices(mesh_asset, skinned_vertices);
    uploaded_mesh = GpuMesh::createFromGeometry(
        allocator, skinned_vertices.data(),
        static_cast<VkDeviceSize>(skinned_vertices.size() *
                                  sizeof(SkinnedMeshVertex)),
        mesh_asset.getIndices().data(), mesh_asset.getIndexCount());
  } else {
    uploaded_mesh = GpuMesh::create(allocator, mesh_asset);
  }

  if (!uploaded_mesh) {
    return nullptr;
  }

  GpuMesh* uploaded_mesh_ptr = uploaded_mesh.get();
  m_gpu_meshes[cache_key] = eastl::move(uploaded_mesh);
  return uploaded_mesh_ptr;
}

bool MeshPreviewOffscreenBackend::renderMeshPreview(
    const MeshAsset& mesh, const MeshPreviewRenderRequest& request,
    const MeshPreviewCameraFrame& framing,
    const MeshPreviewStudioLights& lights, MeshPreviewPoseMode pose_mode,
    eastl::vector<uint8_t>& out_rgba) {
  (void)pose_mode;

  out_rgba.clear();
  m_last_submitted_draw_count = 0;
  if (!framing.ok || !ensureResources(request.width, request.height) ||
      m_forward_path == nullptr) {
    return false;
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanContext* context = backend->nativeVulkanContext();
  VulkanAllocator* allocator = backend->nativeAllocator();
  if (context == nullptr || allocator == nullptr) {
    return false;
  }

  eastl::vector<MeshPreviewSubmeshDraw> submesh_draws;
  if (m_asset_manager != nullptr && !request.mesh_virtual_path.empty()) {
    submesh_draws =
        collectMeshPreviewSubmeshes(*m_asset_manager, request.mesh_virtual_path);
  }
  if (submesh_draws.empty()) {
    MeshPreviewSubmeshDraw fallback_draw{};
    fallback_draw.mesh = eastl::shared_ptr<MeshAsset>(
        const_cast<MeshAsset*>(&mesh), [](MeshAsset*) {});
    fallback_draw.material = mesh.getMaterialAsset();
    submesh_draws.push_back(eastl::move(fallback_draw));
  }

  VulkanTexture* fallback_texture = getFallbackTexture();
  eastl::vector<ForwardOpaqueDraw> opaque_draws;
  eastl::vector<ForwardOpaqueDraw> transparent_draws;
  opaque_draws.reserve(submesh_draws.size());
  transparent_draws.reserve(submesh_draws.size());

  uint32_t slot_index = 0;
  for (const MeshPreviewSubmeshDraw& submesh_draw : submesh_draws) {
    if (!submesh_draw.mesh) {
      continue;
    }
    eastl::string cache_key = submesh_draw.mesh->getVirtualPath();
    if (cache_key.empty()) {
      char suffix[32];
      std::snprintf(suffix, sizeof(suffix), "mesh_preview/anonymous#%u",
                    slot_index);
      cache_key = suffix;
    }
    GpuMesh* gpu_mesh = getOrUploadGpuMesh(*submesh_draw.mesh, cache_key);
    VulkanTexture* base_color_texture = fallback_texture;
    VulkanTexture* metallic_roughness_texture = fallback_texture;
    VulkanTexture* normal_texture = fallback_texture;
    VulkanTexture* occlusion_texture = fallback_texture;
    if (submesh_draw.material) {
      const MaterialAsset& material = *submesh_draw.material;
      if (VulkanTexture* uploaded = ensureTextureUploaded(
              material.getBaseColorTextureAsset().get())) {
        base_color_texture = uploaded;
      }
      if (VulkanTexture* uploaded = ensureTextureUploaded(
              material.getMetallicRoughnessTextureAsset().get())) {
        metallic_roughness_texture = uploaded;
      }
      if (VulkanTexture* uploaded =
              ensureTextureUploaded(material.getNormalTextureAsset().get())) {
        normal_texture = uploaded;
      }
      if (VulkanTexture* uploaded =
              ensureTextureUploaded(material.getOcclusionTextureAsset().get())) {
        occlusion_texture = uploaded;
      }
    }
    appendForwardDraw(submesh_draw, gpu_mesh, base_color_texture,
                      metallic_roughness_texture, normal_texture,
                      occlusion_texture, slot_index, opaque_draws,
                      transparent_draws);
    ++slot_index;
  }

  m_last_submitted_draw_count =
      static_cast<uint32_t>(opaque_draws.size() + transparent_draws.size());

  const ForwardFrameState frame_state = buildMeshPreviewForwardFrameState(
      framing, lights, request.width, request.height);

  VkCommandBuffer command_buffer = context->beginImmediateCommands();
  m_forward_path->renderFrameTo(
      m_offscreen.get(), command_buffer, frame_state, opaque_draws.data(),
      static_cast<uint32_t>(opaque_draws.size()), transparent_draws.data(),
      static_cast<uint32_t>(transparent_draws.size()), 0u, false);

  vulkan_backend::VulkanCommandList command_list;
  command_list.bind(context, command_buffer);
  m_offscreen->transitionToCopySource(command_list);

  auto* vk_target = static_cast<vulkan_backend::VulkanOffscreenTarget*>(
      m_offscreen.get());
  OffscreenRenderTarget* native_target = vk_target->nativeTarget();
  if (native_target == nullptr) {
    context->endImmediateCommands(command_buffer);
    return false;
  }

  VkBufferImageCopy copy_region{};
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageExtent = {request.width, request.height, 1};
  vkCmdCopyImageToBuffer(command_buffer, native_target->getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         m_readback_staging->getBuffer(), 1, &copy_region);
  m_offscreen->transitionToShaderRead(command_list);
  context->endImmediateCommands(command_buffer);

  const VkDeviceSize byte_count =
      static_cast<VkDeviceSize>(request.width) * request.height * 4u;
  vmaInvalidateAllocation(allocator->getAllocator(),
                          m_readback_staging->getAllocation(), 0, byte_count);
  void* mapped = nullptr;
  if (vmaMapMemory(allocator->getAllocator(),
                   m_readback_staging->getAllocation(),
                   &mapped) != VK_SUCCESS ||
      mapped == nullptr) {
    return false;
  }

  out_rgba.resize(static_cast<size_t>(byte_count));
  std::memcpy(out_rgba.data(), mapped, static_cast<size_t>(byte_count));
  vmaUnmapMemory(allocator->getAllocator(),
                 m_readback_staging->getAllocation());
  return true;
}

bool MeshPreviewOffscreenBackend::renderSubmeshDraws(
    const eastl::vector<MeshPreviewSubmeshDraw>& draws,
    const MeshPreviewCameraFrame& framing,
    const MeshPreviewStudioLights& lights, uint32_t width, uint32_t height,
    eastl::vector<uint8_t>& out_rgba, const SceneInstance* lighting_scene) {
  out_rgba.clear();
  m_last_submitted_draw_count = 0;
  if (!framing.ok || draws.empty() || !ensureResources(width, height) ||
      m_forward_path == nullptr) {
    return false;
  }

  auto* backend =
      static_cast<vulkan_backend::VulkanRenderBackend*>(m_render_backend);
  VulkanContext* context = backend->nativeVulkanContext();
  VulkanAllocator* allocator = backend->nativeAllocator();
  if (context == nullptr || allocator == nullptr) {
    return false;
  }

  VulkanTexture* fallback_texture = getFallbackTexture();
  eastl::vector<ForwardOpaqueDraw> opaque_draws;
  eastl::vector<ForwardOpaqueDraw> transparent_draws;
  opaque_draws.reserve(draws.size());
  transparent_draws.reserve(draws.size());

  uint32_t slot_index = 0;
  for (const MeshPreviewSubmeshDraw& submesh_draw : draws) {
    if (!submesh_draw.mesh) {
      continue;
    }
    eastl::string cache_key = submesh_draw.mesh->getVirtualPath();
    if (cache_key.empty()) {
      char suffix[32];
      std::snprintf(suffix, sizeof(suffix), "scene_thumb/anonymous#%u",
                    slot_index);
      cache_key = suffix;
    }
    GpuMesh* gpu_mesh = getOrUploadGpuMesh(*submesh_draw.mesh, cache_key);
    if (gpu_mesh == nullptr) {
      ++slot_index;
      continue;
    }
    VulkanTexture* base_color_texture = fallback_texture;
    VulkanTexture* metallic_roughness_texture = fallback_texture;
    VulkanTexture* normal_texture = fallback_texture;
    VulkanTexture* occlusion_texture = fallback_texture;
    if (submesh_draw.material) {
      const MaterialAsset& material = *submesh_draw.material;
      if (VulkanTexture* uploaded = ensureTextureUploaded(
              material.getBaseColorTextureAsset().get())) {
        base_color_texture = uploaded;
      }
      if (VulkanTexture* uploaded = ensureTextureUploaded(
              material.getMetallicRoughnessTextureAsset().get())) {
        metallic_roughness_texture = uploaded;
      }
      if (VulkanTexture* uploaded =
              ensureTextureUploaded(material.getNormalTextureAsset().get())) {
        normal_texture = uploaded;
      }
      if (VulkanTexture* uploaded =
              ensureTextureUploaded(material.getOcclusionTextureAsset().get())) {
        occlusion_texture = uploaded;
      }
    }
    appendForwardDraw(submesh_draw, gpu_mesh, base_color_texture,
                      metallic_roughness_texture, normal_texture,
                      occlusion_texture, slot_index, opaque_draws,
                      transparent_draws);
    ++slot_index;
  }

  if (opaque_draws.empty() && transparent_draws.empty()) {
    return false;
  }

  m_last_submitted_draw_count =
      static_cast<uint32_t>(opaque_draws.size() + transparent_draws.size());

  ForwardFrameState frame_state =
      buildMeshPreviewForwardFrameState(framing, lights, width, height);
  if (lighting_scene != nullptr) {
    bool has_light = false;
    lighting_scene->forEachLight([&](EntityId, const LightComponent&) {
      has_light = true;
    });
    if (has_light) {
      frame_state.live_scene_lighting = true;
      frame_state.lighting_scene = lighting_scene;
      frame_state.shadow_caster_id = pickDirectionalShadowCaster(*lighting_scene);
      frame_state.shadows_enabled = isValid(frame_state.shadow_caster_id);
    }
  }

  VkCommandBuffer command_buffer = context->beginImmediateCommands();
  m_forward_path->renderFrameTo(
      m_offscreen.get(), command_buffer, frame_state, opaque_draws.data(),
      static_cast<uint32_t>(opaque_draws.size()), transparent_draws.data(),
      static_cast<uint32_t>(transparent_draws.size()), 0u, false);

  vulkan_backend::VulkanCommandList command_list;
  command_list.bind(context, command_buffer);
  m_offscreen->transitionToCopySource(command_list);

  auto* vk_target = static_cast<vulkan_backend::VulkanOffscreenTarget*>(
      m_offscreen.get());
  OffscreenRenderTarget* native_target = vk_target->nativeTarget();
  if (native_target == nullptr) {
    context->endImmediateCommands(command_buffer);
    return false;
  }

  VkBufferImageCopy copy_region{};
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageExtent = {width, height, 1};
  vkCmdCopyImageToBuffer(command_buffer, native_target->getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         m_readback_staging->getBuffer(), 1, &copy_region);
  m_offscreen->transitionToShaderRead(command_list);
  context->endImmediateCommands(command_buffer);

  const VkDeviceSize byte_count =
      static_cast<VkDeviceSize>(width) * height * 4u;
  vmaInvalidateAllocation(allocator->getAllocator(),
                          m_readback_staging->getAllocation(), 0, byte_count);
  void* mapped = nullptr;
  if (vmaMapMemory(allocator->getAllocator(),
                   m_readback_staging->getAllocation(),
                   &mapped) != VK_SUCCESS ||
      mapped == nullptr) {
    return false;
  }

  out_rgba.resize(static_cast<size_t>(byte_count));
  std::memcpy(out_rgba.data(), mapped, static_cast<size_t>(byte_count));
  vmaUnmapMemory(allocator->getAllocator(),
                 m_readback_staging->getAllocation());
  return true;
}

}  // namespace Blunder
