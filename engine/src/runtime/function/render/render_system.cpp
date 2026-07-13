#include "runtime/function/render/render_system.h"

#include <limits>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_opaque_draw.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/opaque_mesh_draw.h"
#include "runtime/function/render/forward/forward_render_path.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/function/render/overlay/overlay_system.h"
#include "runtime/function/render/post/ssao_pass.h"
#include "runtime/function/render/shadow/shadow_map_target.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <slang.h>

#include <algorithm>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "EASTL/memory.h"
#include "runtime/core/base/macro.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/function/render/debug/renderdoc_capture.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/function/ui/ui_host.h"
#include "runtime/function/ui/viewport/i_viewport_sink.h"
#include "runtime/function/ui/viewport/ui_viewport_bridge.h"
#include "runtime/function/ui/viewport/viewport_cpu_frame.h"
#include "runtime/function/ui/viewport/viewport_vulkan_image.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/editor/viewport_pick_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include <vulkan/vulkan.h>

#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/rhi/i_render_backend.h"
#include "runtime/function/render/rhi/render_backend_factory.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"
#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"
#include "runtime/function/render/vulkan_backend/vulkan_render_backend.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/function/scene/mesh_renderer_component.h"

namespace Blunder {

namespace {

const uint64_t k_fence_wait_timeout_ns = 1000000000ULL;

constexpr uint32_t k_default_viewport_w = 1024;
constexpr uint32_t k_default_viewport_h = 720;
constexpr uint32_t k_smoke_texture_size = 64;
constexpr float k_shadow_ortho_half_extent = 14.0f;
constexpr float k_shadow_near_plane = 0.1f;
constexpr float k_shadow_far_plane = 60.0f;

bool viewportZeroCopyDisabled() {
  const char* env = std::getenv("BLUNDER_VIEWPORT_ZERO_COPY");
  return env != nullptr && (env[0] == '0' || env[0] == 'f' || env[0] == 'F');
}

bool editorShadowsEnabled() {
  const char* env = std::getenv("BLUNDER_EDITOR_SHADOWS");
  return env != nullptr && (env[0] == '1' || env[0] == 't' || env[0] == 'T');
}

bool editorOverlayAaEnabled() {
  const char* env = std::getenv("BLUNDER_EDITOR_OVERLAY_AA");
  return env != nullptr && (env[0] == '1' || env[0] == 't' || env[0] == 'T');
}

float editorRenderScale() {
  const char* env = std::getenv("BLUNDER_EDITOR_RENDER_SCALE");
  if (env == nullptr || env[0] == '\0') {
    return 0.85f;
  }
  const float scale = static_cast<float>(std::atof(env));
  return std::clamp(scale, 0.25f, 1.0f);
}

bool matricesNearlyEqual(const glm::mat4& a, const glm::mat4& b) {
  const float* pa = &a[0][0];
  const float* pb = &b[0][0];
  for (int i = 0; i < 16; ++i) {
    const float scale = std::max({1.0f, std::fabs(pa[i]), std::fabs(pb[i])});
    if (std::fabs(pa[i] - pb[i]) > 1e-4f * scale) {
      return false;
    }
  }
  return true;
}

eastl::vector<uint8_t> buildSmokeCheckerboardPixels(uint32_t width,
                                                    uint32_t height) {
  eastl::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4u);

  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const bool dark_tile = (((x / 8u) + (y / 8u)) & 1u) == 0u;
      const size_t index =
          (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u;
      pixels[index + 0] = dark_tile ? 36u : 220u;
      pixels[index + 1] = dark_tile ? 92u : 184u;
      pixels[index + 2] = dark_tile ? 208u : 52u;
      pixels[index + 3] = 255u;
    }
  }

  return pixels;
}

vulkan_backend::VulkanRenderBackend* vkBackend(RenderSystem* self) {
  return static_cast<vulkan_backend::VulkanRenderBackend*>(
      self->getRenderBackend());
}

VulkanContext* vkCtx(RenderSystem* self) {
  return vkBackend(self)->nativeVulkanContext();
}

VulkanAllocator* vkAlloc(RenderSystem* self) {
  return vkBackend(self)->nativeAllocator();
}

VulkanSync* vkSync(RenderSystem* self) {
  return vkBackend(self)->nativeSync();
}

OffscreenRenderTarget* vkOffscreenRt(RenderSystem* self) {
  auto* target = static_cast<vulkan_backend::VulkanOffscreenTarget*>(
      self->getOffscreenTarget());
  return target ? target->nativeTarget() : nullptr;
}

}  // namespace

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() { shutdown(); }

bool RenderSystem::isVulkanBackend() const {
  return m_backend && m_backend->type() == rhi::RenderBackendType::Vulkan;
}

void RenderSystem::initializeBackend(const RenderSystemInitInfo& info) {
  if (m_backend) {
    return;  // Backend already created (e.g. early, to share device with Slint).
  }
  ASSERT(info.window_system);

  m_asset_manager = info.asset_manager;
  m_window_system = info.window_system;
  m_viewport_layout_source = info.viewport_layout_source;
  m_preview_settings_source = info.preview_settings_source;
  m_viewport_bridge = info.viewport_bridge;
  m_viewport_sink = info.viewport_sink;

  rhi::RenderBackendInitInfo backend_init{};
  backend_init.device_desc.window_system = info.window_system;
  backend_init.device_desc.enable_validation = info.enable_validation;
  m_backend = rhi::RenderBackendFactory::createFromSettings(backend_init);
}

void RenderSystem::initialize(const RenderSystemInitInfo& info) {
  ASSERT(info.window_system);
  initializeBackend(info);

  if (m_backend->type() == rhi::RenderBackendType::D3D12) {
    initializeD3D12SkeletonPath(info);
    return;
  }
  initializeVulkanPath(info);
}

SharedVulkanHandles RenderSystem::getSharedVulkanHandles() const {
  SharedVulkanHandles handles{};
  if (!isVulkanBackend()) {
    return handles;
  }
  VulkanContext* ctx = vkCtx(const_cast<RenderSystem*>(this));
  if (!ctx) {
    return handles;
  }
  handles.instance = reinterpret_cast<uint64_t>(ctx->getInstance());
  handles.physical_device = reinterpret_cast<uint64_t>(ctx->getPhysicalDevice());
  handles.device = reinterpret_cast<uint64_t>(ctx->getDevice());
  handles.graphics_queue_family = ctx->getGraphicsQueueFamily();
  handles.valid = handles.instance != 0 && handles.physical_device != 0 &&
                  handles.device != 0;
  return handles;
}

void RenderSystem::initializeD3D12SkeletonPath(
    const RenderSystemInitInfo& info) {
  (void)info;
  LOG_WARN(
      "[RenderSystem] D3D12 skeleton backend: scene pipelines are not "
      "implemented in P0");

  rhi::OffscreenTargetDesc offscreen_desc{};
  offscreen_desc.width = k_default_viewport_w;
  offscreen_desc.height = k_default_viewport_h;
  m_offscreen = m_backend->device().createOffscreenTarget(offscreen_desc);
  m_editor_camera = eastl::make_unique<EditorCamera>(m_window_system);
}

void RenderSystem::initializeVulkanPath(const RenderSystemInitInfo& info) {
  (void)info;

  rhi::OffscreenTargetDesc offscreen_desc{};
  offscreen_desc.width = k_default_viewport_w;
  offscreen_desc.height = k_default_viewport_h;
  m_offscreen = vkBackend(this)->device().createOffscreenTarget(offscreen_desc);

  rhi::GraphicsPipelineDesc mesh_pipeline_desc{};
  mesh_pipeline_desc.shader_path = "engine/shaders/pbr.slang";
  mesh_pipeline_desc.enable_vertex_input = true;
  mesh_pipeline_desc.cull_mode = rhi::CullMode::None;
  mesh_pipeline_desc.enable_depth_test = true;
  mesh_pipeline_desc.enable_depth_write = true;
  mesh_pipeline_desc.enable_texture_sampling = true;
  mesh_pipeline_desc.enable_shadow_sampling = true;
  mesh_pipeline_desc.enable_pbr_texture_sampling = true;
  m_mesh_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_mesh_pipeline->bind(vkCtx(this), vkBackend(this)->nativeSlangCompiler());
  m_mesh_pipeline->initialize(*m_offscreen, mesh_pipeline_desc);

  rhi::GraphicsPipelineDesc transparent_pipeline_desc = mesh_pipeline_desc;
  transparent_pipeline_desc.enable_blend = true;
  transparent_pipeline_desc.enable_depth_write = false;
  transparent_pipeline_desc.cull_mode = rhi::CullMode::None;
  transparent_pipeline_desc.shared_descriptor_set_layout = reinterpret_cast<uint64_t>(
      m_mesh_pipeline->nativePipeline()->getDescriptorSetLayout());
  m_transparent_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_transparent_pipeline->bind(vkCtx(this), vkBackend(this)->nativeSlangCompiler());
  m_transparent_pipeline->initialize(*m_offscreen, transparent_pipeline_desc);

  m_shadow_map = eastl::make_unique<ShadowMapTarget>();
  m_shadow_map->initialize(vkCtx(this), vkAlloc(this));

  rhi::GraphicsPipelineDesc shadow_pipeline_desc{};
  shadow_pipeline_desc.shader_path = "engine/shaders/shadow_depth.slang";
  shadow_pipeline_desc.enable_vertex_input = true;
  shadow_pipeline_desc.cull_mode = rhi::CullMode::Back;
  shadow_pipeline_desc.enable_depth_test = true;
  shadow_pipeline_desc.enable_depth_write = true;
  shadow_pipeline_desc.depth_compare_op = rhi::CompareOp::Less;
  shadow_pipeline_desc.depth_only_subpass = true;
  m_shadow_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_shadow_pipeline->bind(vkCtx(this), vkBackend(this)->nativeSlangCompiler());
  m_shadow_pipeline->initializeWithRenderPass(m_shadow_map->getRenderPass(),
                                              shadow_pipeline_desc);

  m_overlay_system = eastl::make_unique<OverlaySystem>();
  m_overlay_system->initialize(vkCtx(this), vkAlloc(this),
                               m_offscreen.get(),
                               vkBackend(this)->nativeSlangCompiler());

  m_editor_camera = eastl::make_unique<EditorCamera>(m_window_system);

  Asset::Meta smoke_meta;
  smoke_meta.virtual_path = "generated://render/smoke_checkerboard";
  Texture2DAsset smoke_asset(smoke_meta, k_smoke_texture_size,
                             k_smoke_texture_size, 4u,
                             buildSmokeCheckerboardPixels(k_smoke_texture_size,
                                                          k_smoke_texture_size));
  m_fallback_texture = ensureTextureUploaded(&smoke_asset);

  if (m_preview_settings_source != nullptr) {
    m_preview_settings_source->setBlinnPhongMaterialSource(m_inspector_material.get());
    m_preview_settings_source->syncBlinnPhongFromMaterialSource();
  }

  m_forward_path = eastl::make_unique<ForwardRenderPath>();
  ForwardRenderPathInit forward_init{};
  forward_init.vk_context = vkCtx(this);
  forward_init.vk_allocator = vkAlloc(this);
  forward_init.offscreen = m_offscreen.get();
  forward_init.overlay_system = m_overlay_system.get();
  forward_init.opaque_pipeline = m_mesh_pipeline.get();
  forward_init.transparent_pipeline = m_transparent_pipeline.get();
  forward_init.shadow_pipeline = m_shadow_pipeline.get();
  forward_init.shadow_map = m_shadow_map.get();
  forward_init.fallback_texture = m_fallback_texture;
  m_forward_path->initialize(forward_init);

  m_ssao_pass = eastl::make_unique<SsaOPass>();
  m_ssao_pass->initialize(vkCtx(this), vkAlloc(this),
                          vkBackend(this)->nativeSlangCompiler());
  m_ssao_pass->resize(k_default_viewport_w, k_default_viewport_h);

  LOG_INFO(
      "[RenderSystem] PBR pipelines ready (descriptor layout shared: opaque={}, "
      "transparent={})",
      reinterpret_cast<void*>(m_mesh_pipeline->nativePipeline()->getDescriptorSetLayout()),
      reinterpret_cast<void*>(m_transparent_pipeline->nativePipeline()->getDescriptorSetLayout()));

  if (m_viewport_bridge) {
    m_viewport_bridge->initialize(vkCtx(this), vkAlloc(this), vkSync(this));
    resizeViewportReadback(k_default_viewport_w, k_default_viewport_h);
  }

  // Best-effort RenderDoc hookup. If the engine wasn't launched from
  // RenderDoc, this is a silent no-op; otherwise F11 will capture one frame.
  m_renderdoc_capture = eastl::make_unique<RenderDocCapture>();
  m_renderdoc_capture->initialize();
}

GpuMesh* RenderSystem::getOrUploadGpuMesh(const MeshAsset* mesh_asset) {
  if (mesh_asset == nullptr || !isVulkanBackend() || !vkAlloc(this)) {
    return nullptr;
  }

  eastl::string cache_key = mesh_asset->getVirtualPath();
  if (cache_key.empty()) {
    const std::filesystem::path& absolute_path = mesh_asset->getAbsolutePath();
    if (!absolute_path.empty()) {
      cache_key = eastl::string(absolute_path.generic_string().c_str());
    } else {
      cache_key = "generated://render/anonymous_mesh";
    }
  }

  return getOrUploadGpuMeshByKey(cache_key, mesh_asset->getVertexData(),
                                 mesh_asset->getVertexByteSize(),
                                 mesh_asset->getIndices().data(),
                                 mesh_asset->getIndexCount());
}

GpuMesh* RenderSystem::getOrUploadGpuMeshByKey(const eastl::string& cache_key,
                                               const void* vertex_bytes,
                                               size_t vertex_byte_size,
                                               const uint32_t* indices,
                                               size_t index_count) {
  if (cache_key.empty() || vertex_bytes == nullptr || indices == nullptr ||
      vertex_byte_size == 0 || index_count == 0 || !isVulkanBackend() ||
      !vkAlloc(this)) {
    return nullptr;
  }

  if (auto it = m_gpu_meshes.find(cache_key); it != m_gpu_meshes.end()) {
    return it->second.get();
  }

  auto uploaded_mesh = GpuMesh::createFromGeometry(
      vkAlloc(this), vertex_bytes, static_cast<VkDeviceSize>(vertex_byte_size),
      indices, index_count);
  if (!uploaded_mesh) {
    LOG_ERROR("[RenderSystem] GpuMesh upload failed for {}", cache_key.c_str());
    return nullptr;
  }

  GpuMesh* uploaded_mesh_ptr = uploaded_mesh.get();
  m_gpu_meshes[cache_key] = eastl::move(uploaded_mesh);
  LOG_INFO("[RenderSystem] GpuMesh uploaded {} (indices={})", cache_key.c_str(),
           index_count);

  return uploaded_mesh_ptr;
}

bool RenderSystem::addOpaqueMeshDraw(
    GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
    VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
    VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
    const glm::mat4& model, float alpha_cutoff, cgltf_alpha_mode alpha_mode,
    bool double_sided) {
  if (gpu_mesh == nullptr || gpu_mesh->getVertexBuffer() == nullptr ||
      gpu_mesh->getIndexBuffer() == nullptr || gpu_mesh->getIndexCount() == 0) {
    return false;
  }

  if (m_opaque_mesh_draws.size() + m_transparent_mesh_draws.size() >=
      ForwardRenderPath::k_max_opaque_draws) {
    LOG_ERROR("[RenderSystem] mesh draw limit reached ({})",
              ForwardRenderPath::k_max_opaque_draws);
    return false;
  }

  OpaqueMeshDraw draw{};
  draw.gpu_mesh = gpu_mesh;
  draw.material = eastl::move(material);
  draw.base_color_texture = base_color_texture;
  draw.metallic_roughness_texture = metallic_roughness_texture;
  draw.normal_texture = normal_texture;
  draw.occlusion_texture = occlusion_texture;
  draw.model = model;
  draw.alpha_cutoff = alpha_cutoff;
  draw.alpha_mode = alpha_mode;
  draw.double_sided = double_sided;
  draw.slot_index = static_cast<uint32_t>(m_opaque_mesh_draws.size());
  m_opaque_mesh_draws.push_back(eastl::move(draw));
  return true;
}

bool RenderSystem::addTransparentMeshDraw(
    GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
    VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
    VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
    const glm::mat4& model, float alpha_cutoff, bool double_sided) {
  if (gpu_mesh == nullptr || gpu_mesh->getVertexBuffer() == nullptr ||
      gpu_mesh->getIndexBuffer() == nullptr || gpu_mesh->getIndexCount() == 0) {
    return false;
  }

  if (m_opaque_mesh_draws.size() + m_transparent_mesh_draws.size() >=
      ForwardRenderPath::k_max_opaque_draws) {
    LOG_ERROR("[RenderSystem] mesh draw limit reached ({})",
              ForwardRenderPath::k_max_opaque_draws);
    return false;
  }

  OpaqueMeshDraw draw{};
  draw.gpu_mesh = gpu_mesh;
  draw.material = eastl::move(material);
  draw.base_color_texture = base_color_texture;
  draw.metallic_roughness_texture = metallic_roughness_texture;
  draw.normal_texture = normal_texture;
  draw.occlusion_texture = occlusion_texture;
  draw.model = model;
  draw.alpha_cutoff = alpha_cutoff;
  draw.alpha_mode = cgltf_alpha_mode_blend;
  draw.double_sided = double_sided;
  draw.is_transparent = true;
  draw.slot_index =
      static_cast<uint32_t>(m_opaque_mesh_draws.size() + m_transparent_mesh_draws.size());
  m_transparent_mesh_draws.push_back(eastl::move(draw));
  return true;
}

void RenderSystem::markViewportRenderDirty() { ++m_viewport_render_generation; }

void RenderSystem::pollViewportPickIfActive() {
  if (!m_overlay_system || !m_editor_camera ||
      !g_runtime_global_context.m_viewport_pick ||
      !g_runtime_global_context.m_scene_system) {
    return;
  }
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    return;
  }
  m_overlay_system->pollHybridPick(*m_editor_camera, *scene, *this,
                                   *g_runtime_global_context.m_viewport_pick);
}

bool RenderSystem::usesZeroCopyViewport() const {
  return !viewportZeroCopyDisabled() && m_viewport_layout_source != nullptr &&
         m_viewport_layout_source->viewportUsesSharedDevice();
}

void RenderSystem::resetZeroCopyPresentState() {
  for (ZeroCopyPresentSlot& slot : m_zero_copy_slots) {
    slot = {};
  }
  m_zero_copy_last_presented_generation = 0;
  m_zero_copy_next_generation = 1;
}

void RenderSystem::notifyZeroCopySubmitted(const uint32_t slot,
                                           const uint32_t width,
                                           const uint32_t height) {
  if (slot >= VulkanSync::k_max_frames_in_flight) {
    return;
  }
  ZeroCopyPresentSlot& present_slot = m_zero_copy_slots[slot];
  present_slot.width = width;
  present_slot.height = height;
  present_slot.pending_gpu = true;
  present_slot.completed_generation = m_zero_copy_next_generation++;
}

void RenderSystem::pollZeroCopyAndPresent() {
  if (!usesZeroCopyViewport() || !m_viewport_sink || !isVulkanBackend()) {
    return;
  }

  OffscreenRenderTarget* offscreen = vkOffscreenRt(this);
  if (offscreen == nullptr) {
    return;
  }

  VkDevice device = vkCtx(this)->getDevice();
  uint32_t best_slot = UINT32_MAX;
  uint64_t best_generation = 0;
  uint32_t best_width = 0;
  uint32_t best_height = 0;

  for (uint32_t slot = 0; slot < VulkanSync::k_max_frames_in_flight; ++slot) {
    ZeroCopyPresentSlot& present_slot = m_zero_copy_slots[slot];
    if (!present_slot.pending_gpu || present_slot.width == 0 ||
        present_slot.height == 0) {
      continue;
    }

    VkFence fence = vkSync(this)->getInFlightFence(slot);
    if (vkGetFenceStatus(device, fence) != VK_SUCCESS) {
      continue;
    }

    if (present_slot.completed_generation <=
        m_zero_copy_last_presented_generation) {
      present_slot.pending_gpu = false;
      continue;
    }

    if (present_slot.completed_generation > best_generation) {
      best_generation = present_slot.completed_generation;
      best_slot = slot;
      best_width = present_slot.width;
      best_height = present_slot.height;
    }
  }

  if (best_slot == UINT32_MAX) {
    return;
  }

  ViewportVulkanImage vk_image{};
  vk_image.image = reinterpret_cast<uint64_t>(offscreen->getImage(best_slot));
  vk_image.format = static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM);
  vk_image.layout =
      static_cast<uint32_t>(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  vk_image.width = best_width;
  vk_image.height = best_height;
  m_viewport_sink->presentViewportVulkanImage(vk_image);

  m_zero_copy_last_presented_generation = best_generation;
  for (ZeroCopyPresentSlot& present_slot : m_zero_copy_slots) {
    if (present_slot.pending_gpu &&
        present_slot.completed_generation <= best_generation) {
      present_slot.pending_gpu = false;
    }
  }
}

bool RenderSystem::tryBeginRecordingSlot(const uint32_t slot) {
  if (!isVulkanBackend() || slot >= VulkanSync::k_max_frames_in_flight) {
    return false;
  }
  VkDevice device = vkCtx(this)->getDevice();
  VkFence fence = vkSync(this)->getInFlightFence(slot);
  const VkResult fence_status = vkGetFenceStatus(device, fence);
  if (fence_status == VK_NOT_READY) {
    return false;
  }
  if (fence_status != VK_SUCCESS) {
    vkWaitForFences(device, 1, &fence, VK_TRUE, k_fence_wait_timeout_ns);
  }
  vkResetFences(device, 1, &fence);
  return true;
}

void RenderSystem::pollViewportPresent() {
  if (!m_viewport_sink) {
    return;
  }
  if (usesZeroCopyViewport()) {
    pollZeroCopyAndPresent();
  } else if (m_viewport_bridge) {
    m_viewport_bridge->pollAndPresent(m_viewport_sink);
  }
}

void RenderSystem::clearOpaqueMeshDraws() { m_opaque_mesh_draws.clear(); }

void RenderSystem::clearTransparentMeshDraws() {
  m_transparent_mesh_draws.clear();
}

void RenderSystem::clearGpuMeshes() {
  for (auto& [key, mesh] : m_gpu_meshes) {
    if (mesh) {
      mesh->destroy();
      mesh.reset();
    }
  }
  m_gpu_meshes.clear();
}

void RenderSystem::resizeViewportReadback(uint32_t width, uint32_t height) {
  if (m_viewport_bridge) {
    m_viewport_bridge->resizeReadback(width, height);
  }
  resetZeroCopyPresentState();
}

VulkanTexture* RenderSystem::ensureTextureUploaded(
    const Texture2DAsset* texture_asset) {
  if (texture_asset == nullptr || !isVulkanBackend() || !vkAlloc(this)) {
    return nullptr;
  }

  eastl::string cache_key = texture_asset->getVirtualPath();
  if (cache_key.empty()) {
    const std::filesystem::path& absolute_path = texture_asset->getAbsolutePath();
    if (!absolute_path.empty()) {
      cache_key = eastl::string(absolute_path.generic_string().c_str());
    } else {
      cache_key = "generated://render/anonymous_texture";
    }
  }

  if (auto it = m_uploaded_textures.find(cache_key);
      it != m_uploaded_textures.end()) {
    return it->second.get();
  }

  auto uploaded_texture = eastl::make_unique<VulkanTexture>();
  uploaded_texture->createFromTexture2DAsset(vkCtx(this), vkAlloc(this),
                                             *texture_asset);
  VulkanTexture* uploaded_texture_ptr = uploaded_texture.get();
  m_uploaded_textures[cache_key] = eastl::move(uploaded_texture);

  LOG_INFO("[RenderSystem] texture uploaded {} ({}x{}, {} bytes)",
           cache_key.c_str(), texture_asset->getWidth(),
           texture_asset->getHeight(), texture_asset->getPixelByteSize());
  return uploaded_texture_ptr;
}

void RenderSystem::resizeOffscreenIfNeeded(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0 || !m_offscreen) {
    return;
  }
  const rhi::Extent2D current = m_offscreen->extent();
  if (current.width == width && current.height == height) {
    m_deferred_rt_width = 0;
    m_deferred_rt_height = 0;
    return;
  }

  if (m_deferred_rt_width != width || m_deferred_rt_height != height) {
    m_deferred_rt_stable_frames = 0;
  }
  m_deferred_rt_width = width;
  m_deferred_rt_height = height;
}

void RenderSystem::applyDeferredOffscreenResize() {
  if (!m_offscreen || m_deferred_rt_width == 0 || m_deferred_rt_height == 0) {
    return;
  }

  const rhi::Extent2D current = m_offscreen->extent();
  if (current.width == m_deferred_rt_width &&
      current.height == m_deferred_rt_height) {
    m_deferred_rt_width = 0;
    m_deferred_rt_height = 0;
    return;
  }

  if (++m_deferred_rt_stable_frames < 2u) {
    return;
  }

  const uint32_t width = m_deferred_rt_width;
  const uint32_t height = m_deferred_rt_height;
  m_deferred_rt_width = 0;
  m_deferred_rt_height = 0;
  m_deferred_rt_stable_frames = 0;

  if (isVulkanBackend()) {
    vkDeviceWaitIdle(vkCtx(this)->getDevice());
    resizeViewportReadback(width, height);
  }
  m_offscreen->resize(width, height);
  if (isVulkanBackend()) {
    if (auto* vk_target =
            static_cast<vulkan_backend::VulkanOffscreenTarget*>(m_offscreen.get())) {
      vk_target->setActiveBufferIndex(0);
    }
    if (m_ssao_pass) {
      m_ssao_pass->resize(width, height);
    }
  }
  if (m_overlay_system) {
    m_overlay_system->resize(width, height);
  }
  markViewportRenderDirty();
  m_force_viewport_render = true;
}

void RenderSystem::flushOffscreenResizeToTarget(uint32_t target_width,
                                                uint32_t target_height) {
  if (!m_offscreen || target_width == 0 || target_height == 0) {
    return;
  }
  const rhi::Extent2D current = m_offscreen->extent();
  if (current.width == target_width && current.height == target_height) {
    return;
  }
  if (m_viewport_layout_source != nullptr &&
      static_cast<SlintSystem*>(m_viewport_layout_source)
          ->shouldDeferHeavyFrameWork()) {
    return;
  }
  m_deferred_rt_width = target_width;
  m_deferred_rt_height = target_height;
  m_deferred_rt_stable_frames = 1u;
  applyDeferredOffscreenResize();
}

void RenderSystem::shutdown() {
  if (!m_backend) {
    return;
  }

  if (isVulkanBackend()) {
    vkDeviceWaitIdle(vkCtx(this)->getDevice());
    resetZeroCopyPresentState();
  }

  if (m_renderdoc_capture) {
    m_renderdoc_capture->shutdown();
    m_renderdoc_capture.reset();
  }

  for (auto& [key, texture] : m_uploaded_textures) {
    if (texture) {
      texture->destroy();
      texture.reset();
    }
  }
  m_uploaded_textures.clear();

  if (m_ssao_pass) {
    m_ssao_pass->shutdown();
    m_ssao_pass.reset();
  }

  if (m_forward_path) {
    m_forward_path->shutdown();
    m_forward_path.reset();
  }

  clearOpaqueMeshDraws();
  clearTransparentMeshDraws();
  clearGpuMeshes();
  m_inspector_material.reset();
  m_fallback_texture = nullptr;

  if (m_offscreen) {
    if (auto* vk_target =
            static_cast<vulkan_backend::VulkanOffscreenTarget*>(m_offscreen.get())) {
      vk_target->shutdown();
    }
    m_offscreen.reset();
  }

  if (m_overlay_system) {
    m_overlay_system->shutdown();
    m_overlay_system.reset();
  }

  if (m_mesh_pipeline) {
    m_mesh_pipeline->shutdown();
    m_mesh_pipeline.reset();
  }

  if (m_transparent_pipeline) {
    m_transparent_pipeline->shutdown();
    m_transparent_pipeline.reset();
  }

  if (m_shadow_pipeline) {
    m_shadow_pipeline->shutdown();
    m_shadow_pipeline.reset();
  }

  if (m_shadow_map) {
    m_shadow_map->shutdown();
    m_shadow_map.reset();
  }

  m_editor_camera.reset();

  m_backend.reset();
  m_viewport_bridge = nullptr;
  m_viewport_sink = nullptr;

  m_window_system = nullptr;
  m_asset_manager = nullptr;
  m_current_frame = 0;
}

void RenderSystem::requestSceneCameraFocus() {
  m_pending_scene_camera_focus = true;
  m_refocus_when_mesh_draws_ready = true;
}

void RenderSystem::tick(float delta_time, uint32_t target_width,
                        uint32_t target_height) {
  if (!m_backend || !m_offscreen) {
    return;
  }
  if (m_backend->type() == rhi::RenderBackendType::D3D12) {
    tickD3D12Skeleton(delta_time, target_width, target_height);
    return;
  }
  tickVulkan(delta_time, target_width, target_height);
}

void RenderSystem::tickD3D12Skeleton(float delta_time, uint32_t target_width,
                                     uint32_t target_height) {
  (void)delta_time;
  resizeOffscreenIfNeeded(target_width, target_height);
  flushOffscreenResizeToTarget(target_width, target_height);
  applyDeferredOffscreenResize();
  const rhi::Extent2D extent = m_offscreen->extent();
  if (extent.width == 0 || extent.height == 0) {
    return;
  }
  if (m_viewport_sink) {
    eastl::vector<uint8_t> black_pixels(
        static_cast<size_t>(extent.width) * extent.height * 4u, 0u);
    ViewportCpuFrame frame{};
    frame.pixels_rgba = black_pixels.data();
    frame.width = extent.width;
    frame.height = extent.height;
    frame.stride_bytes = extent.width * 4u;
    m_viewport_sink->presentViewportCpuFrame(frame);
  }
}

void RenderSystem::tickVulkan(float delta_time, uint32_t target_width,
                                uint32_t target_height) {
  const float render_scale = editorRenderScale();
  if (render_scale < 0.999f) {
    target_width = eastl::max(
        16u, static_cast<uint32_t>(static_cast<float>(target_width) * render_scale));
    target_height = eastl::max(
        16u, static_cast<uint32_t>(static_cast<float>(target_height) * render_scale));
  }

  resizeOffscreenIfNeeded(target_width, target_height);
  flushOffscreenResizeToTarget(target_width, target_height);
  applyDeferredOffscreenResize();

  const rhi::Extent2D offscreen_extent_rhi = m_offscreen->extent();
  const VkExtent2D offscreen_extent{offscreen_extent_rhi.width,
                                    offscreen_extent_rhi.height};
  if (offscreen_extent.width == 0 || offscreen_extent.height == 0) {
    return;
  }
  if (target_width > 0 && target_height > 0 &&
      (offscreen_extent.width != target_width ||
       offscreen_extent.height != target_height)) {
    return;
  }

  if (m_overlay_system && g_runtime_global_context.m_scene_system) {
    SceneInstance* scene =
        g_runtime_global_context.m_scene_system->getActiveInstance();
    if (scene != nullptr) {
      m_overlay_system->rebuildPickInstancesIfNeeded(*scene, *this);
    }
  }

  if (g_runtime_global_context.m_viewport_pick) {
    g_runtime_global_context.m_viewport_pick->tickDeferredPickRequest();
  }

  pollViewportPickIfActive();

  VkInstance instance = vkCtx(this)->getInstance();
  if (m_renderdoc_capture) {
    m_renderdoc_capture->beginFrame(instance);
  }

  glm::mat4 view(1.0f);
  glm::mat4 projection(1.0f);
  EditorCamera::ProjectionMode projection_mode =
      EditorCamera::ProjectionMode::perspective;
  glm::vec3 camera_position(2.0f, 2.0f, 2.0f);
  glm::vec3 camera_forward = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
  float camera_distance = 6.0f;
  float near_clip = 0.1f;
  float far_clip = 1000.0f;
  float vertical_fov = glm::radians(45.0f);
  float ortho_size = 10.0f;
  if (m_editor_camera) {
    int32_t viewport_x = 0;
    int32_t viewport_y = 0;
    float viewport_logical_w = static_cast<float>(offscreen_extent.width);
    float viewport_logical_h = static_cast<float>(offscreen_extent.height);
    if (m_viewport_layout_source) {
      const ViewportLogicalRect viewport_rect =
          m_viewport_layout_source->getViewportLogicalRect();
      viewport_x = viewport_rect.x;
      viewport_y = viewport_rect.y;
      if (viewport_rect.width > 0 && viewport_rect.height > 0) {
        viewport_logical_w = static_cast<float>(viewport_rect.width);
        viewport_logical_h = static_cast<float>(viewport_rect.height);
      }
    }
    m_editor_camera->setViewportRect(
        viewport_x, viewport_y, viewport_logical_w, viewport_logical_h,
        static_cast<float>(offscreen_extent.width),
        static_cast<float>(offscreen_extent.height));
    m_editor_camera->onUpdate(delta_time);

    const bool wants_scene_focus =
        m_pending_scene_camera_focus || m_refocus_when_mesh_draws_ready;
    if (wants_scene_focus && g_runtime_global_context.m_scene_system != nullptr) {
      SceneInstance* active_scene =
          g_runtime_global_context.m_scene_system->getActiveInstance();
      const bool has_mesh_draws =
          !m_opaque_mesh_draws.empty() || !m_transparent_mesh_draws.empty();
      if (active_scene != nullptr && active_scene->hasWorldBounds() &&
          has_mesh_draws) {
        const AABB& bounds = active_scene->getWorldBounds();
        m_editor_camera->snapFocusOnAABB(bounds);
        m_pending_scene_camera_focus = false;
        m_refocus_when_mesh_draws_ready = false;
      }
    }

    view = m_editor_camera->getViewMatrix();
    projection = m_editor_camera->getProjectionMatrix();
    projection_mode = m_editor_camera->getProjectionMode();
    camera_position = m_editor_camera->getPosition();
    camera_forward = m_editor_camera->getForwardDirection();
    camera_distance = m_editor_camera->getDistance();
    near_clip = m_editor_camera->getNearClip();
    far_clip = m_editor_camera->getFarClip();
    vertical_fov = m_editor_camera->getVerticalFov();
    ortho_size = m_editor_camera->getOrthoSize();
  } else {
    view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                       kWorldUp);
    const float aspect = static_cast<float>(offscreen_extent.width) /
                         static_cast<float>(offscreen_extent.height);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
    proj[1][1] *= -1.0f;
    projection = proj;
    far_clip = 10.0f;
  }

  const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
  if (keyboard_state) {
    if (keyboard_state[SDL_SCANCODE_1]) {
      m_grid_plane = ForwardGridPlane::xy;
    } else if (keyboard_state[SDL_SCANCODE_2]) {
      m_grid_plane = ForwardGridPlane::xz;
    } else if (keyboard_state[SDL_SCANCODE_3]) {
      m_grid_plane = ForwardGridPlane::yz;
    }
  }

  ForwardFrameState frame_state{};
  frame_state.view = view;
  frame_state.projection = projection;
  frame_state.camera_position = camera_position;
  frame_state.camera_forward = camera_forward;
  frame_state.camera_distance = camera_distance;
  frame_state.near_clip = near_clip;
  frame_state.far_clip = far_clip;
  frame_state.vertical_fov = vertical_fov;
  frame_state.ortho_size = ortho_size;
  frame_state.projection_mode = projection_mode;
  frame_state.projection_transition_t = m_editor_camera ? m_editor_camera->getProjectionTransitionT() : 0.0f;
  frame_state.grid_plane = m_grid_plane;
  frame_state.viewport_width = offscreen_extent.width;
  frame_state.viewport_height = offscreen_extent.height;
  if (m_viewport_layout_source != nullptr) {
    auto* slint_layout = static_cast<SlintSystem*>(m_viewport_layout_source);
    slint_layout->syncViewportProjectionMode(
        projection_mode == EditorCamera::ProjectionMode::perspective);
  }
  if (m_preview_settings_source != nullptr) {
    frame_state.shading = m_preview_settings_source->previewSettings().get();
  }
  // Slint UI defaults SSAO on; disable until AO generate is stable for large scenes.
  frame_state.shading.ssao_enabled = false;
  // Editor viewport: shadow pass duplicates every opaque draw (expensive on Sponza).
  // Opt in with BLUNDER_EDITOR_SHADOWS=1.
  frame_state.shadows_enabled = editorShadowsEnabled();
  if (!m_opaque_mesh_draws.empty() || !m_transparent_mesh_draws.empty()) {
    frame_state.shading.ambient_color =
        glm::max(frame_state.shading.ambient_color, glm::vec3(0.35f));
    frame_state.shading.diffuse_color =
        glm::max(frame_state.shading.diffuse_color, glm::vec3(0.9f));
  }

  glm::vec3 shadow_focus(0.0f);
  float shadow_ortho_half_extent = k_shadow_ortho_half_extent;
  if (g_runtime_global_context.m_scene_system != nullptr) {
    SceneInstance* active_scene =
        g_runtime_global_context.m_scene_system->getActiveInstance();
    if (active_scene != nullptr && active_scene->hasWorldBounds()) {
      const AABB& bounds = active_scene->getWorldBounds();
      shadow_focus = bounds.center();
      shadow_ortho_half_extent = computeShadowOrthoHalfExtentFromAABB(
          bounds, frame_state.shading.light_direction);
    }
  }

  computeDirectionalLightMatrices(
      frame_state.shading.light_direction, shadow_focus, shadow_ortho_half_extent,
      k_shadow_near_plane, k_shadow_far_plane, frame_state.light_view,
      frame_state.light_projection, frame_state.light_view_projection);

  const bool viewport_target_changed =
      target_width != m_last_viewport_target_w ||
      target_height != m_last_viewport_target_h;
  const bool camera_changed =
      !matricesNearlyEqual(view, m_last_viewport_view) ||
      !matricesNearlyEqual(projection, m_last_viewport_projection);
  const bool scene_changed =
      m_viewport_render_generation != m_last_rendered_viewport_generation;
  bool skip_camera_only_zero_copy = false;
  if (usesZeroCopyViewport() && m_viewport_layout_source != nullptr &&
      !m_force_viewport_render && !viewport_target_changed && !scene_changed &&
      camera_changed) {
    skip_camera_only_zero_copy =
        !static_cast<SlintSystem*>(m_viewport_layout_source)
             ->isViewportPacingInteractive() &&
        !static_cast<SlintSystem*>(m_viewport_layout_source)
             ->wouldScheduleViewportComposite();
  }
  if (!m_force_viewport_render && !viewport_target_changed && !camera_changed &&
      !scene_changed) {
    pollViewportPresent();
    return;
  }

  if (skip_camera_only_zero_copy) {
    pollViewportPresent();
    return;
  }

  if (!tryBeginRecordingSlot(m_current_frame)) {
    pollViewportPresent();
    if (m_viewport_render_generation != m_last_rendered_viewport_generation) {
      m_force_viewport_render = true;
    }
    return;
  }

  if (OffscreenRenderTarget* offscreen = vkOffscreenRt(this)) {
    offscreen->setActiveBufferIndex(m_current_frame);
  }

  const auto append_forward_draw =
      [&](const OpaqueMeshDraw& mesh_draw, eastl::vector<ForwardOpaqueDraw>& out) {
        if (mesh_draw.gpu_mesh == nullptr) {
          return;
        }

        VulkanBuffer* vertex_buffer = mesh_draw.gpu_mesh->getVertexBuffer();
        VulkanBuffer* index_buffer = mesh_draw.gpu_mesh->getIndexBuffer();
        const uint32_t index_count = mesh_draw.gpu_mesh->getIndexCount();
        if (vertex_buffer == nullptr || index_buffer == nullptr || index_count == 0) {
          return;
        }

        const glm::mat4& model = mesh_draw.model;
        ForwardOpaqueDraw draw{};
        draw.slot_index = mesh_draw.slot_index;
        draw.vertex_buffer = vertex_buffer;
        draw.index_buffer = index_buffer;
        draw.index_count = index_count;
        draw.model = model;
        draw.normal_matrix =
            glm::mat4(glm::transpose(glm::inverse(glm::mat3(model))));
        draw.material = mesh_draw.material.get();
        draw.base_color_texture = mesh_draw.base_color_texture;
        draw.metallic_roughness_texture = mesh_draw.metallic_roughness_texture;
        draw.normal_texture = mesh_draw.normal_texture;
        draw.occlusion_texture = mesh_draw.occlusion_texture;
        draw.alpha_cutoff = mesh_draw.alpha_cutoff;
        draw.alpha_mode = mesh_draw.alpha_mode;
        draw.double_sided = mesh_draw.double_sided;
        out.push_back(draw);
      };

  eastl::vector<ForwardOpaqueDraw> opaque_draws;
  opaque_draws.reserve(m_opaque_mesh_draws.size());
  for (const OpaqueMeshDraw& mesh_draw : m_opaque_mesh_draws) {
    append_forward_draw(mesh_draw, opaque_draws);
  }

  for (OpaqueMeshDraw& mesh_draw : m_transparent_mesh_draws) {
    const glm::vec3 world_position =
        glm::vec3(mesh_draw.model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    mesh_draw.sort_depth =
        glm::length(world_position - frame_state.camera_position);
  }
  std::sort(m_transparent_mesh_draws.begin(), m_transparent_mesh_draws.end(),
            [](const OpaqueMeshDraw& a, const OpaqueMeshDraw& b) {
              return a.sort_depth > b.sort_depth;
            });

  eastl::vector<ForwardOpaqueDraw> transparent_draws;
  transparent_draws.reserve(m_transparent_mesh_draws.size());
  for (const OpaqueMeshDraw& mesh_draw : m_transparent_mesh_draws) {
    append_forward_draw(mesh_draw, transparent_draws);
  }

  VkFence in_flight_fence = vkSync(this)->getInFlightFence(m_current_frame);

  VkCommandBuffer command_buffer =
      m_mesh_pipeline->nativePipeline()->getCommandBuffer(m_current_frame);
  vkResetCommandBuffer(command_buffer, 0);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(command_buffer, &begin_info);

  if (m_overlay_system) {
    m_overlay_system->begin_sync(frame_state, m_current_frame);
  }

  if (m_forward_path) {
    m_forward_path->renderFrame(
        command_buffer, frame_state, opaque_draws.data(),
        static_cast<uint32_t>(opaque_draws.size()), transparent_draws.data(),
        static_cast<uint32_t>(transparent_draws.size()), m_current_frame);
  }

  if (m_overlay_system && m_overlay_system->hasActiveOutline()) {
    m_overlay_system->draw_outline(command_buffer);
  }

  if (m_overlay_system) {
    if (m_overlay_system->hasActiveOutline()) {
      m_overlay_system->draw_outline(command_buffer);
    }
    if (m_overlay_system->hasActiveLineOverlays()) {
      m_overlay_system->draw_overlay_lines(command_buffer);
      if (editorOverlayAaEnabled()) {
        m_overlay_system->draw_overlay_aa(command_buffer);
      }
    }
  }

  if (m_ssao_pass && frame_state.shading.ssao_enabled) {
    m_ssao_pass->apply(command_buffer, vkOffscreenRt(this), frame_state.shading,
                       projection, near_clip, far_clip);
  }

  if (m_overlay_system) {
    m_overlay_system->draw_screen_overlays(command_buffer);
  }

  vulkan_backend::VulkanCommandList command_list;
  command_list.bind(vkCtx(this), command_buffer);

  // Zero-copy path uses the same async fence polling as CPU readback: submit
  // without blocking, present the newest completed frame via pollZeroCopyAndPresent().
  const bool zero_copy_viewport = usesZeroCopyViewport();

  VulkanBuffer* readback_staging =
      (!zero_copy_viewport && m_viewport_bridge)
          ? m_viewport_bridge->stagingBuffer(m_current_frame)
          : nullptr;

  if (zero_copy_viewport) {
    m_offscreen->transitionToShaderRead(command_list);
  } else {
    m_offscreen->transitionToCopySource(command_list);
    if (readback_staging) {
      VkBufferImageCopy copy_region{};
      copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy_region.imageSubresource.mipLevel = 0;
      copy_region.imageSubresource.baseArrayLayer = 0;
      copy_region.imageSubresource.layerCount = 1;
      copy_region.imageExtent = {offscreen_extent.width, offscreen_extent.height,
                                 1};
      vkCmdCopyImageToBuffer(command_buffer, vkOffscreenRt(this)->getImage(),
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             readback_staging->getBuffer(), 1, &copy_region);
    }
    m_offscreen->transitionToShaderRead(command_list);
  }

  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  vkQueueSubmit(vkCtx(this)->getGraphicsQueue(), 1, &submit_info,
                in_flight_fence);

  if (zero_copy_viewport) {
    notifyZeroCopySubmitted(m_current_frame, offscreen_extent.width,
                            offscreen_extent.height);
    pollViewportPresent();
  } else if (m_viewport_bridge) {
    m_viewport_bridge->notifyGpuSubmitted(m_current_frame, offscreen_extent.width,
                                          offscreen_extent.height);
    // Async readback: do NOT block on the fence here. pollAndPresent() uses
    // vkGetFenceStatus (non-blocking) to check if any previous frame's staging
    // copy has completed. The viewport displays ~1 frame behind the GPU, which
    // eliminates the per-frame CPU stall and pipelines GPU/CPU work.
    pollViewportPresent();
  }

  if (m_viewport_layout_source != nullptr) {
    static_cast<SlintSystem*>(m_viewport_layout_source)->syncViewportProjectionMode(
        projection_mode == EditorCamera::ProjectionMode::perspective);
  }

  if (m_renderdoc_capture) {
    m_renderdoc_capture->endFrame(instance);
  }

  m_last_viewport_view = view;
  m_last_viewport_projection = projection;
  m_last_viewport_target_w = target_width;
  m_last_viewport_target_h = target_height;
  m_last_rendered_viewport_generation = m_viewport_render_generation;
  m_force_viewport_render = false;

  m_current_frame = (m_current_frame + 1) % VulkanSync::k_max_frames_in_flight;
}

void RenderSystem::onEvent(Event& event) {
  if (m_renderdoc_capture && m_renderdoc_capture->isAttached() &&
      event.getEventType() == EventType::KeyPressed) {
    auto& key_event = static_cast<KeyPressedEvent&>(event);
    if (!key_event.isRepeat() && key_event.getKeyCode() == SDLK_F11) {
      m_renderdoc_capture->triggerCapture();
    }
  }

  if (m_overlay_system && m_editor_camera) {
    m_overlay_system->transform_gizmo().controller().onEvent(event,
                                                             *m_editor_camera);
    if (event.handled) {
      return;
    }
  }

  if (m_overlay_system && m_editor_camera &&
      event.getEventType() == EventType::MouseMoved && !event.handled) {
    auto& mouse_event = static_cast<MouseMovedEvent&>(event);
    {
      auto& gizmo_ctrl = m_overlay_system->transform_gizmo().controller();
      const Vec2 pos(mouse_event.getX(), mouse_event.getY());
      bool hover_changed = false;
      if (gizmo_ctrl.getMode() != TransformGizmoMode::none && !gizmo_ctrl.isDragging()) {
        if (m_editor_camera->isWindowPositionInViewport(pos)) {
          hover_changed = gizmo_ctrl.updateHoverFromPointer(pos, *m_editor_camera);
        } else if (gizmo_ctrl.hasHover()) {
          gizmo_ctrl.clearHover();
          hover_changed = true;
        }
      } else if (gizmo_ctrl.hasHover()) {
        gizmo_ctrl.clearHover();
        hover_changed = true;
      }
      if (hover_changed) {
        requestViewportRedraw();
      }
    }
    {
      auto& nav_gizmo = m_overlay_system->navigate_gizmo();
      const Vec2 pos(mouse_event.getX(), mouse_event.getY());
      bool nav_hover_changed = false;
      if (m_editor_camera->isWindowPositionInViewport(pos)) {
        nav_hover_changed = nav_gizmo.updateHoverFromPointer(pos, *m_editor_camera);
      } else if (nav_gizmo.hasHover()) {
        nav_gizmo.clearHover();
        nav_hover_changed = true;
      }
      if (nav_hover_changed) {
        requestViewportRedraw();
      }
    }
  }

  if (m_overlay_system && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    auto& mouse_event = static_cast<MouseButtonPressedEvent&>(event);
    if (mouse_event.getMouseButton() == SDL_BUTTON_LEFT &&
        mouse_event.hasMousePosition()) {
      if (m_overlay_system->navigate_gizmo().tryHandleMouseClick(
              Vec2(mouse_event.getX(), mouse_event.getY()), *m_editor_camera)) {
        event.handled = true;
        return;
      }
    }
  }

  if (!event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    auto& mouse_event = static_cast<MouseButtonPressedEvent&>(event);
    if (mouse_event.hasMousePosition() &&
        m_editor_camera->isWindowPositionInViewport(
            Vec2(mouse_event.getX(), mouse_event.getY())) &&
        g_runtime_global_context.m_viewport_pick) {
      if (mouse_event.getMouseButton() == SDL_BUTTON_RIGHT ||
          mouse_event.getMouseButton() == SDL_BUTTON_MIDDLE) {
        g_runtime_global_context.m_viewport_pick->onCameraInteractionStarted();
      }
    }
  }

  if (!event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseMoved) {
    auto& mouse_event = static_cast<MouseMovedEvent&>(event);
    if (g_runtime_global_context.m_viewport_pick) {
      g_runtime_global_context.m_viewport_pick->onViewportPointerMoved(
          mouse_event.getX(), mouse_event.getY());
    }
  }

  if (!event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    auto& mouse_event = static_cast<MouseButtonPressedEvent&>(event);
    if (mouse_event.getMouseButton() == SDL_BUTTON_RIGHT &&
        mouse_event.hasMousePosition() &&
        m_editor_camera->isWindowPositionInViewport(
            Vec2(mouse_event.getX(), mouse_event.getY())) &&
        g_runtime_global_context.m_viewport_pick) {
      const uint16_t modifiers =
          static_cast<uint16_t>(SDL_GetModState() & 0xFFFFu);
      if (keyModifiersCtrl(modifiers)) {
        g_runtime_global_context.m_viewport_pick->onPiercingMenuRequest(
            mouse_event.getX(), mouse_event.getY(), modifiers);
        event.handled = true;
        return;
      }
    }
  }

  if (!event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    auto& mouse_event = static_cast<MouseButtonPressedEvent&>(event);
    if (mouse_event.getMouseButton() == SDL_BUTTON_LEFT &&
        mouse_event.hasMousePosition() &&
        m_editor_camera->isWindowPositionInViewport(
            Vec2(mouse_event.getX(), mouse_event.getY())) &&
        g_runtime_global_context.m_viewport_pick) {
      g_runtime_global_context.m_viewport_pick->onViewportLeftPressed(
          mouse_event.getX(), mouse_event.getY());
    }
  }

  if (!event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonReleased) {
    auto& mouse_event = static_cast<MouseButtonReleasedEvent&>(event);
    if (mouse_event.getMouseButton() == SDL_BUTTON_LEFT &&
        mouse_event.hasMousePosition() &&
        g_runtime_global_context.m_viewport_pick) {
      const uint16_t modifiers =
          static_cast<uint16_t>(SDL_GetModState() & 0xFFFFu);
      g_runtime_global_context.m_viewport_pick->onViewportLeftReleased(
          mouse_event.getX(), mouse_event.getY(), modifiers);
      if (m_editor_camera->isWindowPositionInViewport(
              Vec2(mouse_event.getX(), mouse_event.getY()))) {
        event.handled = true;
        return;
      }
    }
  }

  if (m_editor_camera) {
    m_editor_camera->onEvent(event);
  }
}

EntityId RenderSystem::pickEntityAtWindowPosition(const float window_x,
                                                  const float window_y) {
  if (!m_overlay_system || !m_editor_camera) {
    return k_invalid_entity_id;
  }
  if (!g_runtime_global_context.m_scene_system) {
    return k_invalid_entity_id;
  }
  SceneInstance* scene = g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    return k_invalid_entity_id;
  }
  return m_overlay_system->pickAtWindowPosition(window_x, window_y, *m_editor_camera,
                                              *scene, *this);
}

eastl::vector<EntityId> RenderSystem::pickAllEntitiesAtWindowPosition(
    const float window_x, const float window_y) {
  eastl::vector<EntityId> hits;
  if (!m_overlay_system || !m_editor_camera) {
    return hits;
  }
  if (!g_runtime_global_context.m_scene_system) {
    return hits;
  }
  SceneInstance* scene = g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    return hits;
  }
  return m_overlay_system->pickAllAtWindowPosition(window_x, window_y, *m_editor_camera,
                                                   *scene, *this);
}

void RenderSystem::requestViewportRedraw() {
  markViewportRenderDirty();
  m_force_viewport_render = true;
}

void RenderSystem::setTransformGizmoMode(const TransformGizmoMode mode) {
  if (m_overlay_system) {
    m_overlay_system->transform_gizmo().controller().setMode(mode);
    requestViewportRedraw();
    if (g_runtime_global_context.m_slint_system) {
      g_runtime_global_context.m_slint_system->syncTransformToolbarFromEngine();
    }
  }
}

TransformGizmoMode RenderSystem::getTransformGizmoMode() const {
  if (m_overlay_system) {
    return m_overlay_system->transform_gizmo().controller().getMode();
  }
  return TransformGizmoMode::none;
}

void RenderSystem::toggleTransformGizmoSpace() {
  if (m_overlay_system) {
    m_overlay_system->transform_gizmo().controller().toggleSpace();
    requestViewportRedraw();
  }
}

bool RenderSystem::isTransformGizmoSpaceGlobal() const {
  if (m_overlay_system) {
    return m_overlay_system->transform_gizmo().controller().getSpace() ==
           GizmoSpace::global;
  }
  return true;
}

bool RenderSystem::isTransformGizmoDragging() const {
  if (m_overlay_system) {
    return m_overlay_system->transform_gizmo().controller().isDragging();
  }
  return false;
}

bool RenderSystem::isTranslateModalSessionActive() const {
  if (m_overlay_system) {
    return m_overlay_system->transform_gizmo()
        .controller()
        .isTranslateModalSessionActive();
  }
  return false;
}

}  // namespace Blunder
