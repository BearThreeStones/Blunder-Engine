#include "runtime/function/render/render_system.h"

#include <limits>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/job/job_system.h"
#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/function/render/clustered/froxel_grid.h"
#include "runtime/function/render/deferred/deferred_render_path.h"
#include "runtime/function/render/frame_graph/frame_graph.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/forward/forward_opaque_draw.h"
#include "runtime/function/render/gpu_driven/gpu_driven_renderer.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/mesh_loader.h"
#include "runtime/function/render/opaque_mesh_draw.h"
#include "runtime/function/render/forward/forward_render_path.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/function/render/overlay/editor_overlay_policy.h"
#include "runtime/function/render/overlay/camera_preview_resolve.h"
#include "runtime/function/render/overlay/camera_preview_rt_size.h"
#include "runtime/function/render/player_authorship_input.h"
#include "runtime/function/render/overlay/overlay_system.h"
#include "runtime/function/render/post/ssao_pass.h"
#include "runtime/function/render/post/volumetric_fog_pass.h"
#include "runtime/function/render/volumetric_fog_math.h"
#include "runtime/function/render/scene_thumbnail/scene_still.h"
#include "runtime/function/render/shadow/mesh_shadow_system.h"
#include "runtime/function/render/shadow/shadow_map_target.h"
#include "runtime/function/render/slang/shader_resource_layout.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>
#include <slang.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "EASTL/memory.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/debug/input_present_trace.h"
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
#include "runtime/function/editor/align_camera_actions.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/viewport_pick_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/debug/frame_timing_service.h"
#include "runtime/function/debug/gpu_timestamp_queries.h"
#include "runtime/function/debug/tracy_vk_instrument.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/scene/light_eval.h"
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
#include "runtime/function/render/texture_loader.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_frame_graph_recorder.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"
#include "runtime/function/render/vulkan_backend/vulkan_imported_gpu_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"
#include "runtime/function/render/vulkan_backend/vulkan_render_backend.h"
#include <vk_mem_alloc.h>
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

GpuTimestampQueries* frameGpuQueries() {
  FrameTimingService* timing = g_runtime_global_context.m_frame_timing.get();
  return timing != nullptr ? timing->gpuQueries() : nullptr;
}

#ifdef TRACY_ENABLE
TracyVkCtx frameTracyVk() {
  FrameTimingService* timing = g_runtime_global_context.m_frame_timing.get();
  return timing != nullptr ? timing->tracyVk() : nullptr;
}
#endif

void publishFrameTimingCounts(const ForwardFrameState& frame_state,
                              uint32_t instance_count, uint32_t draw_count) {
  FrameTimingService* timing = g_runtime_global_context.m_frame_timing.get();
  if (timing == nullptr) {
    return;
  }
  uint32_t lights = 0;
  if (frame_state.lighting_scene != nullptr) {
    frame_state.lighting_scene->forEachLight(
        [&](EntityId, const LightComponent&) { ++lights; });
  }
  timing->setCounts(instance_count, lights, draw_count);
}

void harvestReadyGpuTimestamps(VulkanSync* sync) {
  FrameTimingService* timing = g_runtime_global_context.m_frame_timing.get();
  if (timing == nullptr || sync == nullptr) {
    return;
  }
  for (uint32_t slot = 0; slot < VulkanSync::k_max_frames_in_flight; ++slot) {
    if (sync->slotReached(slot)) {
      timing->harvestGpuSlot(slot);
    }
  }
}

void defaultOffscreenExtent(const RenderSystemInitInfo& info, uint32_t& width,
                            uint32_t& height) {
  if (info.window_system == nullptr) {
    const SceneStillExtent cap = captureStillExtent();
    width = cap.width;
    height = cap.height;
    return;
  }
  width = k_default_viewport_w;
  height = k_default_viewport_h;
}

bool viewportZeroCopyDisabled() {
  const char* env = std::getenv("BLUNDER_VIEWPORT_ZERO_COPY");
  return env != nullptr && (env[0] == '0' || env[0] == 'f' || env[0] == 'F');
}

bool isViewportPickInputPoint(const EditorCamera& camera, float window_x,
                              float window_y) {
  if (!camera.isWindowPositionInViewport(Vec2(window_x, window_y))) {
    return false;
  }
  if (SlintSystem* slint = g_runtime_global_context.m_slint_system.get()) {
    if (slint->probeCameraPreviewPanelAtWindow(window_x, window_y)) {
      return false;
    }
  }
  return true;
}

bool editorShadowsForcedOff() {
  const char* env = std::getenv("BLUNDER_EDITOR_SHADOWS");
  return env != nullptr && (env[0] == '0' || env[0] == 'f' || env[0] == 'F');
}

bool playFrameAabbRequested() {
  const char* env = std::getenv("BLUNDER_PLAY_FRAME_AABB");
  return env != nullptr && (env[0] == '1' || env[0] == 't' || env[0] == 'T');
}

bool parseLookatEnv(const char* env, Vec3& eye, Vec3& target) {
  if (env == nullptr || env[0] == '\0') {
    return false;
  }
  float values[6] = {};
  if (std::sscanf(env, "%f,%f,%f,%f,%f,%f", &values[0], &values[1], &values[2],
                  &values[3], &values[4], &values[5]) != 6) {
    return false;
  }
  eye = Vec3(values[0], values[1], values[2]);
  target = Vec3(values[3], values[4], values[5]);
  return true;
}

bool playUsesWorldAabbCamera() {
  Vec3 unused_eye;
  Vec3 unused_target;
  if (parseLookatEnv(std::getenv("BLUNDER_PLAY_LOOKAT"), unused_eye,
                     unused_target)) {
    return false;
  }
  return playFrameAabbRequested() ||
         (g_runtime_global_context.hostMode() == EngineHostMode::Player &&
          g_runtime_global_context.isHeadless());
}

bool editorOverlayAaEnabled() {
  const char* env = std::getenv("BLUNDER_EDITOR_OVERLAY_AA");
  return env != nullptr && (env[0] == '1' || env[0] == 't' || env[0] == 'T');
}

/// Editor Viewport uses Deferred (GBuffer + clustered froxels). Set
/// `BLUNDER_EDITOR_DEFERRED=0` to force Forward. Player always constructs
/// Deferred. Camera Preview / Mesh Preview / Thumbnail own deferred paths.
bool editorDeferredEnabled() {
  const char* env = std::getenv("BLUNDER_EDITOR_DEFERRED");
  if (env == nullptr || env[0] == '\0') {
    return true;
  }
  return env[0] == '1' || env[0] == 't' || env[0] == 'T';
}

class ViewportSceneAllocator final : public IFrameGraphAllocator {
 public:
  std::unique_ptr<rhi::IGpuTexture> createTexture(
      const FrameGraphResourceDesc&) override {
    return nullptr;
  }
  std::unique_ptr<rhi::IGpuBuffer> createBuffer(
      const FrameGraphResourceDesc&) override {
    return nullptr;
  }
};

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

bool isAlignViewToCameraShortcut(const KeyPressedEvent& key_event) {
  const int key_code = key_event.getKeyCode();
  if (key_code == SDLK_KP_0) {
    return !key_event.isCtrlDown() && !key_event.isAltDown();
  }
  if (key_code == SDLK_0) {
    return key_event.isAltDown() && key_event.isShiftDown() &&
           !key_event.isCtrlDown();
  }
  return false;
}

bool isAlignCameraToViewShortcut(const KeyPressedEvent& key_event) {
  const int key_code = key_event.getKeyCode();
  if (key_code == SDLK_KP_0) {
    return key_event.isCtrlDown() && key_event.isAltDown();
  }
  if (key_code == SDLK_0) {
    return key_event.isCtrlDown() && key_event.isAltDown() &&
           key_event.isShiftDown();
  }
  return false;
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

eastl::string gpuMeshCacheKey(const MeshAsset& mesh_asset) {
  eastl::string cache_key = mesh_asset.getVirtualPath();
  if (cache_key.empty()) {
    const std::filesystem::path& absolute_path = mesh_asset.getAbsolutePath();
    if (!absolute_path.empty()) {
      cache_key = eastl::string(absolute_path.generic_string().c_str());
    } else {
      cache_key = "generated://render/anonymous_mesh";
    }
  }
  return cache_key;
}

}  // namespace

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() { shutdown(); }

void RenderSystem::initializeTextureLoader() {
  m_texture_loader = eastl::make_unique<TextureLoader>();
  TextureLoader::InitInfo info;
  info.job_system = g_runtime_global_context.m_job_system.get();
  if (isVulkanBackend()) {
    VulkanContext* context = vkCtx(this);
    VulkanAllocator* allocator = vkAlloc(this);
    if (context != nullptr && allocator != nullptr &&
        context->getDevice() != VK_NULL_HANDLE) {
      info.context = context;
      info.allocator = allocator;
    }
  }
  m_texture_loader->initialize(info);
  if (g_runtime_global_context.m_mesh_loader) {
    g_runtime_global_context.m_mesh_loader->enableGpu(
        info.allocator != nullptr);
  }
}

bool RenderSystem::isVulkanBackend() const {
  return m_backend && m_backend->type() == rhi::RenderBackendType::Vulkan;
}

void RenderSystem::initializeBackend(const RenderSystemInitInfo& info) {
  if (m_backend) {
    return;  // Backend already created (e.g. early, to share device with Slint).
  }

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
  initializeBackend(info);
  if (!m_backend) {
    LOG_ERROR("[RenderSystem] backend create failed");
    return;
  }

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

bool RenderSystem::vrsAttachmentEnabled() const {
  return m_deferred_path != nullptr && m_deferred_path->vrsAttachmentEnabled();
}

bool RenderSystem::fragmentShadingRateExtensionEnabled() const {
  if (!isVulkanBackend()) {
    return false;
  }
  VulkanContext* ctx = vkCtx(const_cast<RenderSystem*>(this));
  return ctx != nullptr && ctx->fragmentShadingRateEnabled();
}

void RenderSystem::vrsTexelSize(uint32_t* width, uint32_t* height) const {
  uint32_t w = 1;
  uint32_t h = 1;
  if (isVulkanBackend()) {
    VulkanContext* ctx = vkCtx(const_cast<RenderSystem*>(this));
    if (ctx != nullptr) {
      const VkExtent2D texel = ctx->fragmentShadingRateTexelSize();
      w = texel.width == 0 ? 1 : texel.width;
      h = texel.height == 0 ? 1 : texel.height;
    }
  }
  if (width != nullptr) {
    *width = w;
  }
  if (height != nullptr) {
    *height = h;
  }
}

eastl::string RenderSystem::physicalDeviceName() const {
  if (!isVulkanBackend()) {
    return {};
  }
  VulkanContext* ctx = vkCtx(const_cast<RenderSystem*>(this));
  if (ctx == nullptr || ctx->physicalDeviceName() == nullptr) {
    return {};
  }
  return eastl::string(ctx->physicalDeviceName());
}

void RenderSystem::initializeD3D12SkeletonPath(
    const RenderSystemInitInfo& info) {
  LOG_WARN(
      "[RenderSystem] D3D12 skeleton backend: scene pipelines are not "
      "implemented in P0");

  rhi::OffscreenTargetDesc offscreen_desc{};
  defaultOffscreenExtent(info, offscreen_desc.width, offscreen_desc.height);
  m_offscreen = m_backend->device().createOffscreenTarget(offscreen_desc);
  m_editor_camera = eastl::make_unique<EditorCamera>(m_window_system);
  if (g_runtime_global_context.hostMode() == EngineHostMode::Player) {
    m_editor_camera->setInteractionLocked(true);
  }
  initializeTextureLoader();
}

void RenderSystem::initializeVulkanPath(const RenderSystemInitInfo& info) {
  initializeTextureLoader();

  rhi::OffscreenTargetDesc offscreen_desc{};
  defaultOffscreenExtent(info, offscreen_desc.width, offscreen_desc.height);
  m_offscreen = vkBackend(this)->device().createOffscreenTarget(offscreen_desc);

  rhi::GraphicsPipelineDesc mesh_pipeline_desc{};
  mesh_pipeline_desc.shader_path = "engine/shaders/pbr.slang";
  mesh_pipeline_desc.enable_vertex_input = true;
  mesh_pipeline_desc.cull_mode = rhi::CullMode::None;
  mesh_pipeline_desc.enable_depth_test = true;
  mesh_pipeline_desc.enable_depth_write = true;
  fillPbrMeshExpectedBindings(mesh_pipeline_desc.expected_descriptor_bindings,
                              mesh_pipeline_desc.expected_descriptor_sets,
                              &mesh_pipeline_desc.expected_descriptor_binding_count,
                              false,
                              mesh_pipeline_desc.expected_descriptor_kinds);
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

  rhi::GraphicsPipelineDesc skinned_mesh_pipeline_desc = mesh_pipeline_desc;
  skinned_mesh_pipeline_desc.shader_path = "engine/shaders/pbr_skinned.slang";
  skinned_mesh_pipeline_desc.enable_skinned_vertex_input = true;
  fillPbrMeshExpectedBindings(
      skinned_mesh_pipeline_desc.expected_descriptor_bindings,
      skinned_mesh_pipeline_desc.expected_descriptor_sets,
      &skinned_mesh_pipeline_desc.expected_descriptor_binding_count, true,
      skinned_mesh_pipeline_desc.expected_descriptor_kinds);
  m_skinned_mesh_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_skinned_mesh_pipeline->bind(vkCtx(this), vkBackend(this)->nativeSlangCompiler());
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
  m_skinned_transparent_pipeline->bind(vkCtx(this),
                                       vkBackend(this)->nativeSlangCompiler());
  m_skinned_transparent_pipeline->initialize(*m_offscreen,
                                             skinned_transparent_pipeline_desc);

  m_shadow_map = eastl::make_unique<ShadowMapTarget>();
  m_shadow_map->initialize(vkCtx(this), vkAlloc(this));

  m_mesh_shadows = eastl::make_unique<MeshShadowSystem>();
  m_mesh_shadows->initialize(vkCtx(this), vkAlloc(this),
                            vkBackend(this)->nativeSlangCompiler(), true);

  rhi::GraphicsPipelineDesc shadow_pipeline_desc{};
  shadow_pipeline_desc.shader_path = "engine/shaders/shadow_depth.slang";
  shadow_pipeline_desc.enable_vertex_input = true;
  shadow_pipeline_desc.cull_mode = rhi::CullMode::Back;
  shadow_pipeline_desc.enable_depth_test = true;
  shadow_pipeline_desc.enable_depth_write = true;
  shadow_pipeline_desc.depth_compare_op = rhi::CompareOp::Less;
  shadow_pipeline_desc.depth_only_subpass = true;
  fillSequentialExpectedBindings(
      shadow_pipeline_desc.expected_descriptor_bindings,
      &shadow_pipeline_desc.expected_descriptor_binding_count,
      k_shadow_descriptor_binding_count);
  m_shadow_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_shadow_pipeline->bind(vkCtx(this), vkBackend(this)->nativeSlangCompiler());
  m_shadow_pipeline->initializeWithRenderPass(m_shadow_map->getRenderPass(),
                                              shadow_pipeline_desc);

  rhi::GraphicsPipelineDesc skinned_shadow_pipeline_desc{};
  skinned_shadow_pipeline_desc.shader_path =
      "engine/shaders/shadow_depth_skinned.slang";
  skinned_shadow_pipeline_desc.enable_vertex_input = true;
  skinned_shadow_pipeline_desc.enable_skinned_vertex_input = true;
  skinned_shadow_pipeline_desc.cull_mode = rhi::CullMode::Back;
  skinned_shadow_pipeline_desc.enable_depth_test = true;
  skinned_shadow_pipeline_desc.enable_depth_write = true;
  skinned_shadow_pipeline_desc.depth_compare_op = rhi::CompareOp::Less;
  skinned_shadow_pipeline_desc.depth_only_subpass = true;
  fillSequentialExpectedBindings(
      skinned_shadow_pipeline_desc.expected_descriptor_bindings,
      &skinned_shadow_pipeline_desc.expected_descriptor_binding_count,
      k_skinned_shadow_descriptor_binding_count);
  m_skinned_shadow_pipeline =
      eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_skinned_shadow_pipeline->bind(vkCtx(this), vkBackend(this)->nativeSlangCompiler());
  m_skinned_shadow_pipeline->initializeWithRenderPass(
      m_shadow_map->getRenderPass(), skinned_shadow_pipeline_desc);

  m_overlay_system = eastl::make_unique<OverlaySystem>();
  m_overlay_system->initialize(vkCtx(this), vkAlloc(this),
                               m_offscreen.get(),
                               vkBackend(this)->nativeSlangCompiler());

  m_editor_camera = eastl::make_unique<EditorCamera>(m_window_system);
  if (g_runtime_global_context.hostMode() == EngineHostMode::Player) {
    m_editor_camera->setInteractionLocked(true);
  }

  Asset::Meta smoke_meta;
  smoke_meta.virtual_path = "generated://render/smoke_checkerboard";
  Texture2DAsset smoke_asset(smoke_meta, k_smoke_texture_size,
                             k_smoke_texture_size, 4u,
                             buildSmokeCheckerboardPixels(k_smoke_texture_size,
                                                          k_smoke_texture_size));
  m_fallback_texture =
      vkCtx(this)->ensureUploadedTexture(vkAlloc(this), smoke_asset);

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
  forward_init.skinned_opaque_pipeline = m_skinned_mesh_pipeline.get();
  forward_init.skinned_transparent_pipeline = m_skinned_transparent_pipeline.get();
  forward_init.skinned_shadow_pipeline = m_skinned_shadow_pipeline.get();
  forward_init.shadow_map = m_shadow_map.get();
  forward_init.fallback_texture = m_fallback_texture;
  forward_init.mesh_shadows = m_mesh_shadows.get();
  m_forward_path->initialize(forward_init);

  const bool host_is_player =
      g_runtime_global_context.hostMode() == EngineHostMode::Player;
  if (host_is_player || editorDeferredEnabled()) {
    m_deferred_path = eastl::make_unique<DeferredRenderPath>();
    DeferredRenderPathInit deferred_init{};
    deferred_init.vk_context = vkCtx(this);
    deferred_init.vk_allocator = vkAlloc(this);
    deferred_init.slang_compiler = vkBackend(this)->nativeSlangCompiler();
    deferred_init.offscreen = vkOffscreenRt(this);
    deferred_init.forward_path = m_forward_path.get();
    deferred_init.shadow_map = m_shadow_map.get();
    deferred_init.fallback_texture = m_fallback_texture;
    deferred_init.mesh_shadows = m_mesh_shadows.get();
    m_deferred_path->initialize(deferred_init);
    LOG_INFO(
        "[RenderSystem] {} uses the Deferred Render Path "
        "(Placement Preview stays Forward; BLUNDER_EDITOR_DEFERRED=0 forces "
        "only the editor Viewport to Forward)",
        host_is_player ? "Player" : "editor viewport");
  }

  m_ssao_pass = eastl::make_unique<SsaOPass>();
  m_ssao_pass->initialize(vkCtx(this), vkAlloc(this),
                          vkBackend(this)->nativeSlangCompiler());
  m_ssao_pass->resize(offscreen_desc.width, offscreen_desc.height);

  m_volumetric_fog_pass = eastl::make_unique<VolumetricFogPass>();
  m_volumetric_fog_pass->initialize(vkCtx(this), vkAlloc(this),
                                    vkBackend(this)->nativeSlangCompiler());

  m_gpu_driven_renderer = eastl::make_unique<GpuDrivenRenderer>();
  m_gpu_driven_renderer->initialize(
      vkCtx(this), vkAlloc(this), vkBackend(this)->nativeSlangCompiler(),
      vkOffscreenRt(this)->getRenderPass(), m_shadow_map->getRenderPass(),
      m_deferred_path ? m_deferred_path->gbufferRenderPass() : VK_NULL_HANDLE);
  m_gpu_driven_renderer->resizeHiZ(offscreen_desc.width, offscreen_desc.height);
  m_gpu_driven_renderer->setMeshShadows(m_mesh_shadows.get());

  LOG_INFO(
      "[RenderSystem] PBR pipelines ready (descriptor layout shared: opaque={}, "
      "transparent={})",
      reinterpret_cast<void*>(m_mesh_pipeline->nativePipeline()->getDescriptorSetLayout()),
      reinterpret_cast<void*>(m_transparent_pipeline->nativePipeline()->getDescriptorSetLayout()));

  if (m_viewport_bridge) {
    m_viewport_bridge->initialize(vkCtx(this), vkAlloc(this), vkSync(this));
    resizeViewportReadback(offscreen_desc.width, offscreen_desc.height);
  }

  // Best-effort RenderDoc hookup. If the engine wasn't launched from
  // RenderDoc, this is a silent no-op; otherwise F11 will capture one frame.
  m_renderdoc_capture = eastl::make_unique<RenderDocCapture>();
  m_renderdoc_capture->initialize();

  if (g_runtime_global_context.m_frame_timing) {
    g_runtime_global_context.m_frame_timing->attachGpu(vkCtx(this));
  }
}

GpuMesh* RenderSystem::getOrUploadGpuMesh(const MeshAsset* mesh_asset) {
  if (mesh_asset == nullptr || !isVulkanBackend() || !vkAlloc(this)) {
    return nullptr;
  }

  const eastl::string cache_key = gpuMeshCacheKey(*mesh_asset);
  if (cache_key.empty()) {
    return nullptr;
  }
  if (auto it = m_gpu_meshes.find(cache_key); it != m_gpu_meshes.end()) {
    return it->second.get();
  }

  auto uploaded_mesh = GpuMesh::create(vkAlloc(this), *mesh_asset);
  if (!uploaded_mesh) {
    LOG_ERROR("[RenderSystem] GpuMesh upload failed for {}", cache_key.c_str());
    return nullptr;
  }
  GpuMesh* uploaded_mesh_ptr = uploaded_mesh.get();
  m_gpu_meshes[cache_key] = eastl::move(uploaded_mesh);
  LOG_INFO("[RenderSystem] GpuMesh uploaded {} (indices={}, meshlets={})",
           cache_key.c_str(), uploaded_mesh_ptr->getIndexCount(),
           uploaded_mesh_ptr->getMeshletRecords().size());
  return uploaded_mesh_ptr;
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

GpuMesh* RenderSystem::findUploadedGpuMesh(const eastl::string& cache_key) const {
  if (cache_key.empty()) {
    return nullptr;
  }
  const auto it = m_gpu_meshes.find(cache_key);
  if (it == m_gpu_meshes.end()) {
    return nullptr;
  }
  return it->second.get();
}

GpuMesh* RenderSystem::updateOrUploadSkinnedGpuMesh(
    const eastl::string& base_cache_key, const void* vertex_bytes,
    size_t vertex_byte_size, const uint32_t* indices, size_t index_count) {
  if (base_cache_key.empty() || vertex_bytes == nullptr || indices == nullptr ||
      vertex_byte_size == 0 || index_count == 0 || !isVulkanBackend() ||
      !vkAlloc(this)) {
    return nullptr;
  }

  eastl::string cache_key(base_cache_key);
  cache_key.append("#skinned");

  if (auto it = m_gpu_meshes.find(cache_key); it != m_gpu_meshes.end()) {
    GpuMesh* existing_mesh = it->second.get();
    if (existing_mesh != nullptr &&
        existing_mesh->uploadVertices(vertex_bytes, vertex_byte_size)) {
      return existing_mesh;
    }
  }

  return getOrUploadGpuMeshByKey(cache_key, vertex_bytes, vertex_byte_size, indices,
                               index_count);
}

GpuMesh* RenderSystem::gpuMeshForEditorOverlay(const MeshAsset* mesh_asset) {
  if (mesh_asset == nullptr) {
    return nullptr;
  }
  eastl::string skinned_key = gpuMeshCacheKey(*mesh_asset);
  skinned_key.append("#skinned");
  if (GpuMesh* skinned = findUploadedGpuMesh(skinned_key)) {
    return skinned;
  }
  return getOrUploadGpuMesh(mesh_asset);
}

bool RenderSystem::addOpaqueMeshDraw(
    GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
    VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
    VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
    const glm::mat4& model, float alpha_cutoff, cgltf_alpha_mode alpha_mode,
    bool double_sided, eastl::vector<glm::mat4> gpu_bone_palette,
    EntityId entity_id) {
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
  draw.gpu_bone_palette = eastl::move(gpu_bone_palette);
  draw.entity_id = entity_id;
  draw.slot_index = static_cast<uint32_t>(m_opaque_mesh_draws.size());
  m_opaque_mesh_draws.push_back(eastl::move(draw));
  return true;
}

bool RenderSystem::addTransparentMeshDraw(
    GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
    VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
    VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
    const glm::mat4& model, float alpha_cutoff, bool double_sided,
    eastl::vector<glm::mat4> gpu_bone_palette, EntityId entity_id) {
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
  draw.gpu_bone_palette = eastl::move(gpu_bone_palette);
  draw.entity_id = entity_id;
  draw.slot_index =
      static_cast<uint32_t>(m_opaque_mesh_draws.size() + m_transparent_mesh_draws.size());
  m_transparent_mesh_draws.push_back(eastl::move(draw));
  return true;
}

bool RenderSystem::addGpuDrivenDraw(
    GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
    VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
    VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
    const glm::mat4& model, float alpha_cutoff, cgltf_alpha_mode alpha_mode,
    bool double_sided, EntityId entity_id) {
  if (gpu_mesh == nullptr || !gpu_mesh->hasMeshlets() ||
      gpu_mesh->getMeshletIndexBuffer() == nullptr) {
    return false;
  }
  if (m_gpu_driven_draws.size() >= k_max_gpu_driven_instances) {
    LOG_ERROR("[RenderSystem] GPU-driven instance limit reached ({})",
              k_max_gpu_driven_instances);
    return false;
  }
  GpuDrivenDraw draw{};
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
  draw.entity_id = entity_id;
  m_gpu_driven_draws.push_back(eastl::move(draw));
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
  present_slot.submit_ns = SDL_GetTicksNS();
}

void RenderSystem::pollZeroCopyAndPresent() {
  if (!usesZeroCopyViewport() || !m_viewport_sink || !isVulkanBackend()) {
    return;
  }

  OffscreenRenderTarget* offscreen = vkOffscreenRt(this);
  if (offscreen == nullptr) {
    return;
  }

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

    if (!vkSync(this)->slotReached(slot)) {
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
  if (m_viewport_layout_source != nullptr &&
      !static_cast<SlintSystem*>(m_viewport_layout_source)
           ->lastViewportExternalBindOk()) {
    // Dispatch-depth skip (or a failed bind) must not consume pending_gpu.
    // Otherwise the completed Sponza slot is never rebound into the Viewport.
    return;
  }

  if (inputPresentTraceEnabled()) {
    const uint64_t submit_ns = m_zero_copy_slots[best_slot].submit_ns;
    const double gpu_ms =
        submit_ns != 0 ? static_cast<double>(SDL_GetTicksNS() - submit_ns) / 1.0e6
                       : 0.0;
    char data[160];
    std::snprintf(data, sizeof(data),
                  "{\"gen\":%llu,\"slot\":%u,\"gpuMs\":%.2f,\"w\":%u,\"h\":%u}",
                  static_cast<unsigned long long>(best_generation), best_slot,
                  gpu_ms, best_width, best_height);
    traceInputPresent("viewport-bind", data);
  }

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
  if (!vkSync(this)->slotReached(slot)) {
    return false;
  }
  const VkResult wait_result =
      vkSync(this)->waitSlot(slot, k_fence_wait_timeout_ns);
  if (wait_result != VK_SUCCESS) {
    return false;
  }
  if (FrameTimingService* timing = g_runtime_global_context.m_frame_timing.get()) {
    timing->harvestGpuSlot(slot);
  }
  vkCtx(this)->onInFlightFenceRetired(slot);
  SecondaryCommandBufferPool& secondary_pool =
      vkCtx(this)->secondaryCommandBuffers();
  secondary_pool.resetFrame(SecondaryStream::viewport, slot);
  secondary_pool.resetFrame(SecondaryStream::camera_preview, slot);
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

void RenderSystem::clearGpuDrivenDraws() { m_gpu_driven_draws.clear(); }

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

void RenderSystem::shutdownCameraPreviewResources() {
  if (isVulkanBackend() && vkCtx(this)) {
    vkDeviceWaitIdle(vkCtx(this)->getDevice());
  }
  if (m_camera_preview_deferred) {
    m_camera_preview_deferred->shutdown();
    m_camera_preview_deferred.reset();
  }
  if (m_camera_preview_staging_map && m_camera_preview_staging && vkAlloc(this)) {
    vmaUnmapMemory(vkAlloc(this)->getAllocator(),
                   m_camera_preview_staging->getAllocation());
    m_camera_preview_staging_map = nullptr;
  }
  if (m_camera_preview_staging) {
    m_camera_preview_staging->destroy();
    m_camera_preview_staging.reset();
  }
  m_camera_preview_staging_w = 0;
  m_camera_preview_staging_h = 0;
  if (m_camera_preview_offscreen) {
    if (auto* vk_target = static_cast<vulkan_backend::VulkanOffscreenTarget*>(
            m_camera_preview_offscreen.get())) {
      vk_target->shutdown();
    }
    m_camera_preview_offscreen.reset();
  }
  m_camera_preview_readback_pending = false;
}

void RenderSystem::ensureCameraPreviewOffscreen(uint32_t width, uint32_t height) {
  if (!isVulkanBackend() || width == 0 || height == 0) {
    return;
  }
  auto ensure_deferred = [this]() {
    if (m_camera_preview_deferred || !m_camera_preview_offscreen ||
        !m_forward_path) {
      return;
    }
    auto* vk_target = static_cast<vulkan_backend::VulkanOffscreenTarget*>(
        m_camera_preview_offscreen.get());
    OffscreenRenderTarget* native = vk_target ? vk_target->nativeTarget() : nullptr;
    if (native == nullptr) {
      return;
    }
    m_camera_preview_deferred = eastl::make_unique<DeferredRenderPath>();
    DeferredRenderPathInit deferred_init{};
    deferred_init.vk_context = vkCtx(this);
    deferred_init.vk_allocator = vkAlloc(this);
    deferred_init.slang_compiler = vkBackend(this)->nativeSlangCompiler();
    deferred_init.offscreen = native;
    deferred_init.forward_path = m_forward_path.get();
    deferred_init.shadow_map = m_shadow_map.get();
    deferred_init.fallback_texture = m_fallback_texture;
    deferred_init.mesh_shadows = nullptr;
    m_camera_preview_deferred->initialize(deferred_init);
  };

  if (!m_camera_preview_offscreen) {
    rhi::OffscreenTargetDesc desc{};
    desc.width = width;
    desc.height = height;
    m_camera_preview_offscreen =
        vkBackend(this)->device().createOffscreenTarget(desc);
    resizeCameraPreviewReadback(width, height);
    ensure_deferred();
    return;
  }
  const rhi::Extent2D current = m_camera_preview_offscreen->extent();
  if (current.width == width && current.height == height) {
    ensure_deferred();
    return;
  }
  vkDeviceWaitIdle(vkCtx(this)->getDevice());
  if (m_camera_preview_deferred) {
    m_camera_preview_deferred->dropGpuTargets();
  }
  m_camera_preview_offscreen->resize(width, height);
  if (m_camera_preview_deferred) {
    m_camera_preview_deferred->resize(width, height);
  }
  resizeCameraPreviewReadback(width, height);
  ensure_deferred();
}

void RenderSystem::resizeCameraPreviewReadback(uint32_t width, uint32_t height) {
  if (!isVulkanBackend() || !vkAlloc(this) || width == 0 || height == 0) {
    return;
  }
  if (m_camera_preview_staging_map && m_camera_preview_staging) {
    vmaUnmapMemory(vkAlloc(this)->getAllocator(),
                   m_camera_preview_staging->getAllocation());
    m_camera_preview_staging_map = nullptr;
  }
  if (m_camera_preview_staging) {
    m_camera_preview_staging->destroy();
    m_camera_preview_staging.reset();
  }
  const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * 4u;
  m_camera_preview_staging = eastl::make_unique<VulkanBuffer>();
  m_camera_preview_staging->create(vkAlloc(this), bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VMA_MEMORY_USAGE_GPU_TO_CPU);
  const VkResult map_result = vmaMapMemory(
      vkAlloc(this)->getAllocator(), m_camera_preview_staging->getAllocation(),
      &m_camera_preview_staging_map);
  if (map_result != VK_SUCCESS) {
    m_camera_preview_staging_map = nullptr;
    m_camera_preview_readback_pending = false;
  }
  m_camera_preview_staging_w = width;
  m_camera_preview_staging_h = height;
}

void RenderSystem::clearCameraPreviewPresentation() {
  if (m_viewport_layout_source && !m_camera_preview_image_cleared) {
    static_cast<SlintSystem*>(m_viewport_layout_source)->clearCameraPreviewImage();
    m_camera_preview_image_cleared = true;
  }
  m_camera_preview_readback_pending = false;
}

void RenderSystem::ensureCameraPreviewOffscreenIfNeeded() {
  if (!editorOverlaysEnabled(g_runtime_global_context.hostMode()) ||
      !m_viewport_layout_source || !isVulkanBackend()) {
    return;
  }
  const auto* slint = static_cast<const SlintSystem*>(m_viewport_layout_source);
  if (slint->isCameraPreviewCollapsed()) {
    return;
  }
  if (!g_runtime_global_context.m_scene_system ||
      !g_runtime_global_context.m_editor_selection) {
    return;
  }
  SceneInstance* scene = g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    return;
  }
  const EditorSelectionSystem& selection = *g_runtime_global_context.m_editor_selection;
  const eastl::vector<EntityId> selected = selection.getSelectedIds();
  const CameraPreviewTargetResult target = resolveCameraPreviewTarget(
      *scene, selection.getPrimarySelection(), selected);
  if (!target.ok) {
    return;
  }
  const CameraPreviewRtSize rt_size = computeCameraPreviewRtSize(
      slint->getCameraPreviewContentWidth(), slint->getCameraPreviewContentHeight());
  if (!rt_size.ok) {
    return;
  }
  ensureCameraPreviewOffscreen(rt_size.width, rt_size.height);
}

void RenderSystem::syncCameraPreviewSkipClear() {
  if (!editorOverlaysEnabled(g_runtime_global_context.hostMode()) ||
      !m_viewport_layout_source) {
    return;
  }
  auto* slint = static_cast<SlintSystem*>(m_viewport_layout_source);
  if (slint->isCameraPreviewCollapsed()) {
    clearCameraPreviewPresentation();
    return;
  }
  if (!g_runtime_global_context.m_scene_system ||
      !g_runtime_global_context.m_editor_selection) {
    clearCameraPreviewPresentation();
    return;
  }
  SceneInstance* scene = g_runtime_global_context.m_scene_system->getActiveInstance();
  if (scene == nullptr) {
    clearCameraPreviewPresentation();
    return;
  }
  const EditorSelectionSystem& selection = *g_runtime_global_context.m_editor_selection;
  const eastl::vector<EntityId> selected = selection.getSelectedIds();
  const CameraPreviewTargetResult target = resolveCameraPreviewTarget(
      *scene, selection.getPrimarySelection(), selected);
  if (!target.ok) {
    clearCameraPreviewPresentation();
    return;
  }
  const CameraPreviewRtSize rt_size = computeCameraPreviewRtSize(
      slint->getCameraPreviewContentWidth(), slint->getCameraPreviewContentHeight());
  if (!rt_size.ok) {
    clearCameraPreviewPresentation();
    return;
  }
  const float aspect =
      static_cast<float>(rt_size.width) /
      static_cast<float>(eastl::max(1u, rt_size.height));
  const ResolvedPlayCamera cam =
      buildCameraPreviewMatrices(*scene, target.entity_id, aspect);
  if (!cam.ok) {
    clearCameraPreviewPresentation();
  }
}

bool RenderSystem::recordCameraPreviewPass(
    VkCommandBuffer command_buffer, const ForwardFrameState& main_frame_state,
    const eastl::vector<ForwardOpaqueDraw>& opaque_draws,
    const eastl::vector<ForwardOpaqueDraw>& /*transparent_draws*/,
    const uint32_t frame_index, uint32_t& out_width, uint32_t& out_height) {
  out_width = 0;
  out_height = 0;
  if (!editorOverlaysEnabled(g_runtime_global_context.hostMode()) ||
      !m_viewport_layout_source || !m_forward_path) {
    return false;
  }

  auto* slint = static_cast<SlintSystem*>(m_viewport_layout_source);
  if (slint->isCameraPreviewCollapsed()) {
    clearCameraPreviewPresentation();
    return false;
  }

  SceneInstance* scene = g_runtime_global_context.m_scene_system
                             ? g_runtime_global_context.m_scene_system->getActiveInstance()
                             : nullptr;
  if (scene == nullptr || !g_runtime_global_context.m_editor_selection) {
    clearCameraPreviewPresentation();
    return false;
  }

  const EditorSelectionSystem& selection = *g_runtime_global_context.m_editor_selection;
  const eastl::vector<EntityId> selected = selection.getSelectedIds();
  const CameraPreviewTargetResult target = resolveCameraPreviewTarget(
      *scene, selection.getPrimarySelection(), selected);
  if (!target.ok) {
    clearCameraPreviewPresentation();
    return false;
  }

  const CameraPreviewRtSize rt_size = computeCameraPreviewRtSize(
      slint->getCameraPreviewContentWidth(), slint->getCameraPreviewContentHeight());
  if (!rt_size.ok) {
    clearCameraPreviewPresentation();
    return false;
  }

  if (!m_camera_preview_offscreen || !m_camera_preview_staging ||
      !m_camera_preview_staging_map) {
    clearCameraPreviewPresentation();
    return false;
  }

  const float aspect =
      static_cast<float>(rt_size.width) /
      static_cast<float>(eastl::max(1u, rt_size.height));
  const ResolvedPlayCamera cam =
      buildCameraPreviewMatrices(*scene, target.entity_id, aspect);
  if (!cam.ok) {
    clearCameraPreviewPresentation();
    return false;
  }
  m_camera_preview_image_cleared = false;

  ForwardFrameState preview_state = main_frame_state;
  preview_state.view = cam.view;
  preview_state.projection = cam.projection;
  preview_state.camera_position = cam.position;
  preview_state.camera_forward = cam.forward;
  preview_state.near_clip = cam.near_clip;
  preview_state.far_clip = cam.far_clip;
  preview_state.vertical_fov = cam.vertical_fov_radians;
  preview_state.projection_mode = EditorCamera::ProjectionMode::perspective;
  preview_state.shadows_enabled = false;
  preview_state.viewport_width = rt_size.width;
  preview_state.viewport_height = rt_size.height;
  preview_state.shading.froxel_occupancy_heatmap = false;
  preview_state.shading.vrs_rate_mask = false;

  eastl::vector<OpaqueMeshDraw> sorted_transparent = m_transparent_mesh_draws;
  for (OpaqueMeshDraw& mesh_draw : sorted_transparent) {
    const glm::vec3 world_position =
        glm::vec3(mesh_draw.model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    mesh_draw.sort_depth = glm::length(world_position - cam.position);
  }
  std::sort(sorted_transparent.begin(), sorted_transparent.end(),
            [](const OpaqueMeshDraw& a, const OpaqueMeshDraw& b) {
              return a.sort_depth > b.sort_depth;
            });

  eastl::vector<ForwardOpaqueDraw> preview_transparent_draws;
  preview_transparent_draws.reserve(sorted_transparent.size());
  for (const OpaqueMeshDraw& mesh_draw : sorted_transparent) {
    if (mesh_draw.gpu_mesh == nullptr) {
      continue;
    }
    VulkanBuffer* vertex_buffer = mesh_draw.gpu_mesh->getVertexBuffer();
    VulkanBuffer* index_buffer = mesh_draw.gpu_mesh->getIndexBuffer();
    const uint32_t index_count = mesh_draw.gpu_mesh->getIndexCount();
    if (vertex_buffer == nullptr || index_buffer == nullptr || index_count == 0) {
      continue;
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
    preview_transparent_draws.push_back(draw);
  }

  if (!m_camera_preview_deferred) {
    clearCameraPreviewPresentation();
    return false;
  }

  auto* preview_rt = static_cast<vulkan_backend::VulkanOffscreenTarget*>(
      m_camera_preview_offscreen.get());
  if (preview_rt != nullptr) {
    preview_rt->setActiveBufferIndex(frame_index %
                                    OffscreenRenderTarget::k_buffer_count);
  }
  const uint32_t preview_frame =
      frame_index % VulkanSync::k_max_frames_in_flight;
  m_camera_preview_deferred->recordGBufferPass(
      command_buffer, preview_state, opaque_draws.data(),
      static_cast<uint32_t>(opaque_draws.size()), preview_frame,
      m_gpu_driven_renderer.get(), m_gpu_driven_draws.data(),
      static_cast<uint32_t>(m_gpu_driven_draws.size()),
      SecondaryStream::camera_preview);
  m_camera_preview_deferred->recordLightingPass(
      command_buffer, preview_state, opaque_draws.data(),
      static_cast<uint32_t>(opaque_draws.size()),
      preview_transparent_draws.data(),
      static_cast<uint32_t>(preview_transparent_draws.size()), preview_frame,
      m_gpu_driven_renderer.get(), SecondaryStream::camera_preview,
      /*draw_overlays=*/false);

  vulkan_backend::VulkanCommandList command_list;
  command_list.bind(vkCtx(this), command_buffer);
  m_camera_preview_offscreen->transitionToCopySource(command_list);

  OffscreenRenderTarget* native_rt = preview_rt ? preview_rt->nativeTarget() : nullptr;
  if (native_rt != nullptr) {
    VkBufferImageCopy copy_region{};
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageExtent = {rt_size.width, rt_size.height, 1};
    vkCmdCopyImageToBuffer(command_buffer, native_rt->getImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           m_camera_preview_staging->getBuffer(), 1, &copy_region);
  }
  m_camera_preview_offscreen->transitionToShaderRead(command_list);

  out_width = rt_size.width;
  out_height = rt_size.height;
  return true;
}

void RenderSystem::tryPresentCameraPreview() {
  if (!m_camera_preview_readback_pending || !m_viewport_layout_source ||
      !m_camera_preview_staging_map || m_camera_preview_readback_w == 0 ||
      m_camera_preview_readback_h == 0) {
    return;
  }
  auto* slint = static_cast<SlintSystem*>(m_viewport_layout_source);
  if (slint->isCameraPreviewCollapsed()) {
    clearCameraPreviewPresentation();
    return;
  }
  if (!vkSync(this)->slotReached(m_camera_preview_readback_slot)) {
    return;
  }
  static_cast<SlintSystem*>(m_viewport_layout_source)
      ->setCameraPreviewImage(static_cast<const uint8_t*>(m_camera_preview_staging_map),
                              m_camera_preview_readback_w,
                              m_camera_preview_readback_h);
  m_camera_preview_readback_pending = false;
}

VulkanTexture* RenderSystem::ensureTextureUploaded(
    const Texture2DAsset* texture_asset) {
  if (texture_asset == nullptr || !isVulkanBackend() || !vkAlloc(this) ||
      !vkCtx(this)) {
    return nullptr;
  }
  const eastl::string key = gpuTextureCacheKey(*texture_asset);
  if (VulkanTexture* resident = vkCtx(this)->findUploadedTexture(key)) {
    return resident;
  }
  if (texture_asset->getPixelData() != nullptr &&
      texture_asset->getPixelByteSize() > 0 && texture_asset->getWidth() > 0 &&
      texture_asset->getHeight() > 0) {
    return vkCtx(this)->ensureUploadedTexture(vkAlloc(this), *texture_asset);
  }
  if (m_texture_loader) {
    return m_texture_loader->request(texture_asset);
  }
  return vkCtx(this)->ensureUploadedTexture(vkAlloc(this), *texture_asset);
}

uint32_t RenderSystem::textureUploadInFlightCount() const {
  if (!m_texture_loader) {
    return 0u;
  }
  return m_texture_loader->inFlightCount();
}

void RenderSystem::dropInFlightTextures() {
  if (m_texture_loader) {
    m_texture_loader->dropScene();
  }
}

void RenderSystem::dropInFlightMeshes() {
  if (g_runtime_global_context.m_mesh_loader) {
    g_runtime_global_context.m_mesh_loader->dropScene();
  }
}

void RenderSystem::pumpMeshLoader(SceneInstance* scene_instance) {
  MeshLoader* loader = g_runtime_global_context.m_mesh_loader.get();
  if (loader == nullptr) {
    return;
  }
  loader->tick();
  if (scene_instance != nullptr) {
    scene_instance->bindStreamedMeshes(*loader);
  }
  if (!loader->isGpuEnabled() || !isVulkanBackend() || vkAlloc(this) == nullptr) {
    return;
  }
  uint32_t uploaded = 0;
  const uint32_t budget = loader->gpuBudget();
  const eastl::vector<eastl::string> pending = loader->gpuPendingKeys();
  for (const eastl::string& key : pending) {
    const eastl::shared_ptr<MeshAsset> mesh = loader->cpuMesh(key);
    if (!mesh) {
      continue;
    }
    const eastl::string cache_key = gpuMeshCacheKey(*mesh);
    if (findUploadedGpuMesh(cache_key) != nullptr) {
      loader->markGpuUploaded(key);
      continue;
    }
    if (uploaded >= budget) {
      break;
    }
    if (getOrUploadGpuMesh(mesh.get()) != nullptr) {
      loader->markGpuUploaded(key);
      ++uploaded;
    } else {
      loader->markGpuFailed(key);
    }
  }
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
  if (m_deferred_path) {
    m_deferred_path->dropGpuTargets();
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
    if (m_gpu_driven_renderer) {
      m_gpu_driven_renderer->resizeHiZ(width, height);
    }
    if (m_deferred_path) {
      m_deferred_path->resize(width, height);
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

bool RenderSystem::readbackOffscreenRgba(eastl::vector<uint8_t>& out_rgba,
                                         uint32_t& out_width,
                                         uint32_t& out_height) {
  out_rgba.clear();
  out_width = 0;
  out_height = 0;
  if (!isVulkanBackend() || !m_offscreen || !vkBackend(this)) {
    return false;
  }
  VulkanContext* context = vkCtx(this);
  VulkanAllocator* allocator = vkAlloc(this);
  if (context == nullptr || allocator == nullptr) {
    return false;
  }
  const rhi::Extent2D extent = m_offscreen->extent();
  if (extent.width == 0 || extent.height == 0) {
    return false;
  }
  const VkDeviceSize bytes =
      static_cast<VkDeviceSize>(extent.width) * extent.height * 4u;
  if (!m_play_frame_staging || m_play_frame_staging_w != extent.width ||
      m_play_frame_staging_h != extent.height) {
    if (m_play_frame_staging) {
      m_play_frame_staging->destroy();
    } else {
      m_play_frame_staging = eastl::make_unique<VulkanBuffer>();
    }
    m_play_frame_staging->create(allocator, bytes,
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VMA_MEMORY_USAGE_GPU_TO_CPU);
    m_play_frame_staging_w = extent.width;
    m_play_frame_staging_h = extent.height;
  }

  VkCommandBuffer command_buffer = context->beginImmediateCommands();
  vulkan_backend::VulkanCommandList command_list;
  command_list.bind(context, command_buffer);
  m_offscreen->transitionToCopySource(command_list);
  OffscreenRenderTarget* native = vkOffscreenRt(this);
  if (native == nullptr) {
    context->endImmediateCommands(command_buffer);
    return false;
  }
  VkBufferImageCopy copy_region{};
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageExtent = {extent.width, extent.height, 1};
  vkCmdCopyImageToBuffer(command_buffer, native->getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         m_play_frame_staging->getBuffer(), 1, &copy_region);
  m_offscreen->transitionToShaderRead(command_list);
  context->endImmediateCommands(command_buffer);

  vmaInvalidateAllocation(allocator->getAllocator(),
                          m_play_frame_staging->getAllocation(), 0, bytes);
  void* mapped = nullptr;
  if (vmaMapMemory(allocator->getAllocator(),
                   m_play_frame_staging->getAllocation(),
                   &mapped) != VK_SUCCESS ||
      mapped == nullptr) {
    return false;
  }
  out_rgba.resize(static_cast<size_t>(bytes));
  std::memcpy(out_rgba.data(), mapped, static_cast<size_t>(bytes));
  vmaUnmapMemory(allocator->getAllocator(),
                 m_play_frame_staging->getAllocation());
  out_width = extent.width;
  out_height = extent.height;
  return true;
}

void RenderSystem::shutdown() {
  if (!m_backend) {
    return;
  }

  if (m_texture_loader) {
    m_texture_loader->stopAndWaitCpu();
  }

  if (isVulkanBackend()) {
    vkDeviceWaitIdle(vkCtx(this)->getDevice());
    resetZeroCopyPresentState();
    if (m_play_frame_staging) {
      m_play_frame_staging->destroy();
      m_play_frame_staging.reset();
    }
    m_play_frame_staging_w = 0;
    m_play_frame_staging_h = 0;
  }

  if (m_texture_loader) {
    m_texture_loader->shutdown();
    m_texture_loader.reset();
  }

  if (m_renderdoc_capture) {
    m_renderdoc_capture->shutdown();
    m_renderdoc_capture.reset();
  }

  m_fallback_texture = nullptr;

  if (m_gpu_driven_renderer) {
    m_gpu_driven_renderer->shutdown();
    m_gpu_driven_renderer.reset();
  }

  if (m_ssao_pass) {
    m_ssao_pass->shutdown();
    m_ssao_pass.reset();
  }

  if (m_volumetric_fog_pass) {
    m_volumetric_fog_pass->shutdown();
    m_volumetric_fog_pass.reset();
  }

  if (m_deferred_path) {
    m_deferred_path->shutdown();
    m_deferred_path.reset();
  }

  if (m_forward_path) {
    m_forward_path->shutdown();
    m_forward_path.reset();
  }

  if (m_mesh_shadows) {
    m_mesh_shadows->shutdown();
    m_mesh_shadows.reset();
  }

  clearOpaqueMeshDraws();
  clearTransparentMeshDraws();
  clearGpuDrivenDraws();
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

  shutdownCameraPreviewResources();

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

void RenderSystem::invalidateGpuDrivenOcclusion() {
  if (m_gpu_driven_renderer) {
    m_gpu_driven_renderer->invalidateSceneOcclusion();
  }
}

void RenderSystem::notifyActiveSceneChanged() {
  requestViewportRedraw();
  requestSceneCameraFocus();
  if (m_volumetric_fog_pass) {
    m_volumetric_fog_pass->invalidateHistory();
  }
  if (m_gpu_driven_renderer) {
    m_gpu_driven_renderer->invalidateSceneOcclusion();
  }
  if (m_viewport_layout_source != nullptr) {
    static_cast<SlintSystem*>(m_viewport_layout_source)
        ->forceNextViewportImageBind();
  }
}

void RenderSystem::tick(float delta_time, uint32_t target_width,
                        uint32_t target_height) {
  if (m_texture_loader) {
    m_texture_loader->tick();
    if (m_texture_loader->consumeResidencyChanged()) {
      m_defer_viewport_for_texture_residency = true;
    }
  }
  if (g_runtime_global_context.m_mesh_loader) {
    if (g_runtime_global_context.m_mesh_loader->consumeResidencyChanged()) {
      requestViewportRedraw();
    }
  }
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

void RenderSystem::recordViewportGraph(
    VkCommandBuffer command_buffer, const ForwardFrameState& frame_state,
    const ForwardOpaqueDraw* opaque_draws, uint32_t opaque_draw_count,
    const ForwardOpaqueDraw* transparent_draws, uint32_t transparent_draw_count,
    uint32_t frame_index, bool host_is_player) {
  if (!m_forward_path && !m_deferred_path) {
    return;
  }

  OffscreenRenderTarget* offscreen = vkOffscreenRt(this);
  if (offscreen == nullptr) {
    LOG_FATAL("[RenderSystem] viewport graph missing offscreen");
  }
  if (m_shadow_map == nullptr) {
    LOG_FATAL("[RenderSystem] viewport graph missing shadow map");
  }

  vulkan_backend::VulkanImportedGpuTexture color_import;
  vulkan_backend::VulkanImportedGpuTexture depth_import;
  vulkan_backend::VulkanImportedGpuTexture shadow_import;
  color_import.bind(offscreen->getImage(), VK_IMAGE_ASPECT_COLOR_BIT);
  depth_import.bind(offscreen->getDepthImage(), VK_IMAGE_ASPECT_DEPTH_BIT);
  shadow_import.bind(m_shadow_map->getDepthImage(), VK_IMAGE_ASPECT_DEPTH_BIT);
  if (color_import.vkImage() == VK_NULL_HANDLE ||
      depth_import.vkImage() == VK_NULL_HANDLE ||
      shadow_import.vkImage() == VK_NULL_HANDLE) {
    LOG_FATAL("[RenderSystem] viewport graph missing VkImage");
  }

  const VkExtent2D color_extent = offscreen->getExtent();
  const VkExtent2D shadow_extent = m_shadow_map->getExtent();

  FrameGraphResourceDesc color_desc{};
  color_desc.format = FrameGraphFormat::R8G8B8A8_UNORM;
  color_desc.width = color_extent.width;
  color_desc.height = color_extent.height;
  color_desc.sample_count = 1;
  color_desc.mip_count = 1;

  FrameGraphResourceDesc depth_desc = color_desc;
  depth_desc.format = FrameGraphFormat::D32_SFLOAT;

  FrameGraphResourceDesc shadow_desc = depth_desc;
  shadow_desc.width = shadow_extent.width;
  shadow_desc.height = shadow_extent.height;

  FrameGraph graph;
  GraphBuilder builder(graph);
  const FrameGraphHandle color =
      builder.importExternal(color_desc, &color_import, "viewport.color");
  const FrameGraphHandle depth =
      builder.importExternal(depth_desc, &depth_import, "viewport.depth");
  const FrameGraphHandle shadow =
      builder.importExternal(shadow_desc, &shadow_import, "shadow.map");

  auto color_handshake = [&](FrameGraphPassHandle pass) {
    builder.read(pass, color, FrameGraphUsage::Sampled);
    builder.write(pass, color, FrameGraphUsage::ColorAttachment);
    builder.read(pass, color, FrameGraphUsage::Sampled);
  };

  const bool use_deferred = m_deferred_path != nullptr;
  if (use_deferred) {
    const FrameGraphPassHandle gbuffer = builder.addPass("viewport.gbuffer");
    builder.write(gbuffer, depth, FrameGraphUsage::DepthAttachment);
    builder.read(gbuffer, depth, FrameGraphUsage::Sampled);
    builder.setExecute(gbuffer, [&](IFrameGraphRecorder&) {
      GpuPassScope gpu(frameGpuQueries(), command_buffer,
                       GpuTimestampZone::viewport_gbuffer);
      BLUNDER_TRACY_VK_ZONE(frameTracyVk(), command_buffer, "viewport.gbuffer");
      m_deferred_path->recordGBufferPass(
          command_buffer, frame_state, opaque_draws, opaque_draw_count,
          frame_index, m_gpu_driven_renderer.get(), m_gpu_driven_draws.data(),
          static_cast<uint32_t>(m_gpu_driven_draws.size()),
          SecondaryStream::viewport);
    });

    const FrameGraphPassHandle lighting = builder.addPass("viewport.lighting");
    builder.read(lighting, depth, FrameGraphUsage::Sampled);
    builder.read(lighting, shadow, FrameGraphUsage::Sampled);
    builder.write(lighting, color, FrameGraphUsage::ColorAttachment);
    builder.read(lighting, color, FrameGraphUsage::Sampled);
    builder.setExecute(lighting, [&](IFrameGraphRecorder&) {
      GpuPassScope gpu(frameGpuQueries(), command_buffer,
                       GpuTimestampZone::viewport_lighting);
      BLUNDER_TRACY_VK_ZONE(frameTracyVk(), command_buffer, "viewport.lighting");
      m_deferred_path->recordLightingPass(
          command_buffer, frame_state, opaque_draws, opaque_draw_count,
          transparent_draws, transparent_draw_count, frame_index,
          m_gpu_driven_renderer.get(), SecondaryStream::viewport,
          /*draw_overlays=*/!host_is_player);
    });
  } else if (m_forward_path) {
    const FrameGraphPassHandle scene = builder.addPass("viewport.scene");
    builder.write(scene, color, FrameGraphUsage::ColorAttachment);
    builder.read(scene, color, FrameGraphUsage::Sampled);
    builder.write(scene, depth, FrameGraphUsage::DepthAttachment);
    builder.read(scene, depth, FrameGraphUsage::Sampled);
    builder.read(scene, shadow, FrameGraphUsage::Sampled);
    builder.setExecute(scene, [&](IFrameGraphRecorder&) {
      GpuPassScope gpu(frameGpuQueries(), command_buffer,
                       GpuTimestampZone::viewport_scene);
      BLUNDER_TRACY_VK_ZONE(frameTracyVk(), command_buffer, "viewport.scene");
      m_forward_path->renderFrame(
          command_buffer, frame_state, opaque_draws, opaque_draw_count,
          transparent_draws, transparent_draw_count, frame_index,
          m_gpu_driven_renderer.get(), m_gpu_driven_draws.data(),
          static_cast<uint32_t>(m_gpu_driven_draws.size()));
    });
  }

  if (m_overlay_system && m_overlay_system->hasActiveOutline()) {
    const FrameGraphPassHandle outline = builder.addPass("viewport.outline");
    color_handshake(outline);
    builder.setExecute(outline, [&](IFrameGraphRecorder&) {
      m_overlay_system->draw_outline(command_buffer);
    });
  }
  if (m_overlay_system && m_overlay_system->hasActiveLineOverlays()) {
    const FrameGraphPassHandle lines = builder.addPass("viewport.line_aa");
    color_handshake(lines);
    builder.setExecute(lines, [&](IFrameGraphRecorder&) {
      m_overlay_system->draw_overlay_lines(command_buffer);
      if (editorOverlayAaEnabled()) {
        m_overlay_system->draw_overlay_aa(command_buffer);
      }
    });
  }
  if (m_ssao_pass && frame_state.shading.ssao_enabled) {
    const FrameGraphPassHandle ssao = builder.addPass("viewport.ssao");
    color_handshake(ssao);
    builder.read(ssao, depth, FrameGraphUsage::Sampled);
    builder.setExecute(ssao, [&](IFrameGraphRecorder&) {
      GpuPassScope gpu(frameGpuQueries(), command_buffer,
                       GpuTimestampZone::viewport_ssao);
      BLUNDER_TRACY_VK_ZONE(frameTracyVk(), command_buffer, "viewport.ssao");
      m_ssao_pass->apply(command_buffer, offscreen, frame_state.shading,
                         frame_state.projection, frame_state.near_clip,
                         frame_state.far_clip, frame_index);
    });
  }

  ActiveFog active_fog{};
  if (frame_state.lighting_scene != nullptr) {
    active_fog = pickActiveFog(*frame_state.lighting_scene);
  }
  if (m_volumetric_fog_pass &&
      shouldApplyVolumetricFog(g_runtime_global_context.hostMode(),
                               isValid(active_fog.entity_id))) {
    const FrameGraphPassHandle fog_pass = builder.addPass("viewport.volumetric_fog");
    color_handshake(fog_pass);
    builder.read(fog_pass, depth, FrameGraphUsage::Sampled);
    builder.setExecute(fog_pass, [&, active_fog](IFrameGraphRecorder&) {
      GpuPassScope gpu(frameGpuQueries(), command_buffer,
                       GpuTimestampZone::viewport_volumetric_fog);
      BLUNDER_TRACY_VK_ZONE(frameTracyVk(), command_buffer,
                           "viewport.volumetric_fog");
      m_volumetric_fog_pass->apply(command_buffer, offscreen, frame_state, active_fog,
                                   frame_index);
    });
  }
  if (m_overlay_system) {
    const FrameGraphPassHandle screen = builder.addPass("viewport.screen");
    color_handshake(screen);
    builder.setExecute(screen, [&](IFrameGraphRecorder&) {
      m_overlay_system->draw_screen_overlays(command_buffer);
    });
  }

  const FrameGraphPassHandle copy = builder.addPass("viewport.copy");
  builder.read(copy, color, FrameGraphUsage::Sampled);
  builder.markSink(copy);
  builder.setExecute(copy, [&](IFrameGraphRecorder&) {
    GpuPassScope gpu(frameGpuQueries(), command_buffer,
                     GpuTimestampZone::viewport_copy);
    BLUNDER_TRACY_VK_ZONE(frameTracyVk(), command_buffer, "viewport.copy");
    vulkan_backend::VulkanCommandList command_list;
    command_list.bind(vkCtx(this), command_buffer);
    const bool zero_copy_viewport = usesZeroCopyViewport();
    VulkanBuffer* readback_staging =
        (!zero_copy_viewport && m_viewport_bridge)
            ? m_viewport_bridge->stagingBuffer(frame_index)
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
        copy_region.imageExtent = {color_extent.width, color_extent.height, 1};
        vkCmdCopyImageToBuffer(command_buffer, offscreen->getImage(),
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback_staging->getBuffer(), 1, &copy_region);
      }
      m_offscreen->transitionToShaderRead(command_list);
    }
  });

  const FrameGraphCompileResult compiled = graph.compile();
  if (!compiled.ok) {
    LOG_FATAL("[RenderSystem] viewport graph compile failed: {}",
              static_cast<int>(compiled.reason));
  }
  ViewportSceneAllocator allocator;
  const FrameGraphAllocateResult allocated = graph.allocate(allocator);
  if (!allocated.ok) {
    LOG_FATAL("[RenderSystem] viewport graph allocate failed: {}",
              static_cast<int>(allocated.reason));
  }
  const FrameGraphPlanBarriersResult planned = graph.planBarriers();
  if (!planned.ok) {
    LOG_FATAL("[RenderSystem] viewport graph planBarriers failed: {}",
              static_cast<int>(planned.reason));
  }
  vulkan_backend::VulkanFrameGraphRecorder recorder;
  recorder.bind(&graph, command_buffer);
  const FrameGraphExecuteResult executed = graph.execute(recorder);
  if (!executed.ok) {
    LOG_FATAL("[RenderSystem] viewport graph execute failed: {}",
              static_cast<int>(executed.reason));
  }
}

void RenderSystem::tickVulkan(float delta_time, uint32_t target_width,
                                uint32_t target_height) {
  InputPresentPhaseTrace phases("render-phases");
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
  ensureCameraPreviewOffscreenIfNeeded();
  syncCameraPreviewSkipClear();

  tryPresentCameraPreview();
  phases.mark("resizePreviewMs");

  const rhi::Extent2D offscreen_extent_rhi = m_offscreen->extent();
  const VkExtent2D offscreen_extent{offscreen_extent_rhi.width,
                                    offscreen_extent_rhi.height};
  if (offscreen_extent.width == 0 || offscreen_extent.height == 0) {
    return;
  }
  if (m_defer_viewport_for_texture_residency) {
    m_defer_viewport_for_texture_residency = false;
    requestViewportRedraw();
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
  phases.mark("pickMs");

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
  const bool player =
      g_runtime_global_context.hostMode() == EngineHostMode::Player;
  if (player) {
    SceneInstance* scene =
        g_runtime_global_context.m_scene_system
            ? g_runtime_global_context.m_scene_system->getActiveInstance()
            : nullptr;
    const float aspect =
        static_cast<float>(offscreen_extent.width) /
        static_cast<float>(eastl::max(1u, offscreen_extent.height));
    bool used_play_camera = false;
    Vec3 play_lookat_eye;
    Vec3 play_lookat_target;
    const bool has_play_lookat = parseLookatEnv(
        std::getenv("BLUNDER_PLAY_LOOKAT"), play_lookat_eye, play_lookat_target);
    if (has_play_lookat && m_editor_camera != nullptr) {
      m_editor_camera->setViewportRect(
          0, 0, static_cast<float>(offscreen_extent.width),
          static_cast<float>(offscreen_extent.height),
          static_cast<float>(offscreen_extent.width),
          static_cast<float>(offscreen_extent.height));
      m_editor_camera->snapLookAt(play_lookat_eye, play_lookat_target);
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
      used_play_camera = true;
    }
    if (!used_play_camera && playUsesWorldAabbCamera() &&
        m_editor_camera != nullptr && scene != nullptr) {
      m_editor_camera->setViewportRect(
          0, 0, static_cast<float>(offscreen_extent.width),
          static_cast<float>(offscreen_extent.height),
          static_cast<float>(offscreen_extent.width),
          static_cast<float>(offscreen_extent.height));
      if (!scene->hasWorldBounds()) {
        scene->rebuildWorldBoundsFromMeshes();
      }
      if (scene->hasWorldBounds()) {
        m_editor_camera->snapFocusOnAABB(scene->getWorldBounds());
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
        used_play_camera = true;
      }
    }
    if (!used_play_camera) {
      ResolvedPlayCamera cam =
          scene ? resolvePlayCameraFromScene(*scene, aspect)
                : ResolvedPlayCamera{};
      if (!cam.ok) {
        pollViewportPresent();
        return;
      }
      view = cam.view;
      projection = cam.projection;
      camera_position = cam.position;
      camera_forward = cam.forward;
      near_clip = cam.near_clip;
      far_clip = cam.far_clip;
      vertical_fov = cam.vertical_fov_radians;
    }
  } else if (m_editor_camera) {
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

    Vec3 lookat_eye;
    Vec3 lookat_target;
    const bool has_lookat = parseLookatEnv(std::getenv("BLUNDER_EDITOR_LOOKAT"),
                                           lookat_eye, lookat_target);
    const bool wants_scene_focus =
        m_pending_scene_camera_focus || m_refocus_when_mesh_draws_ready;
    if (wants_scene_focus && g_runtime_global_context.m_scene_system != nullptr) {
      SceneInstance* active_scene =
          g_runtime_global_context.m_scene_system->getActiveInstance();
      if (active_scene != nullptr && !active_scene->hasWorldBounds()) {
        active_scene->rebuildWorldBoundsFromMeshes();
      }
      if (active_scene != nullptr && active_scene->hasWorldBounds()) {
        m_editor_camera->snapFocusOnAABB(active_scene->getWorldBounds());
        m_pending_scene_camera_focus = false;
        m_refocus_when_mesh_draws_ready = false;
      }
    }
    // LOOKAT must snap even before mesh draws exist. Waiting on draws plus
    // zero-copy camera-only skip leaves the presented image on the origin
    // grid while Unique gizmos sit on the courtyard.
    static bool s_lookat_applied = false;
    static int s_lookat_hold_frames = 0;
    if (has_lookat && !s_lookat_applied) {
      s_lookat_applied = true;
      s_lookat_hold_frames = 12;
      m_pending_scene_camera_focus = false;
      m_refocus_when_mesh_draws_ready = false;
      m_editor_camera->snapLookAt(lookat_eye, lookat_target);
    }
    if (s_lookat_hold_frames > 0) {
      --s_lookat_hold_frames;
      m_force_viewport_render = true;
    }

    view = m_editor_camera->getViewMatrix();
    projection = m_editor_camera->getProjectionMatrix();
    projection_mode = m_editor_camera->getProjectionMode();
    camera_position = m_editor_camera->getPosition();
    camera_forward = m_editor_camera->getForwardDirection();
    camera_distance = m_editor_camera->getDistance();
    {
      static int s_dump = 0;
      if (s_dump < 8) {
        ++s_dump;
        if (FILE* dump = std::fopen(
                "E:/cursor/stores/bc-a87e1603-397e-40c2-ac5d-d4373b287a4a/"
                "internal/gizmo-cam.log",
                s_dump == 1 ? "w" : "a")) {
          SceneInstance* sc =
              g_runtime_global_context.m_scene_system
                  ? g_runtime_global_context.m_scene_system->getActiveInstance()
                  : nullptr;
          const Vec3 focal = m_editor_camera->getFocalPoint();
          const float cam_far = m_editor_camera->getFarClip();
          float lighting_far = cam_far;
          if (sc != nullptr && sc->hasWorldBounds()) {
            lighting_far = clusteredLightingFar(
                cam_far,
                glm::length(sc->getWorldBounds().max - sc->getWorldBounds().min),
                camera_distance);
          }
          std::fprintf(
              dump,
              "n=%d lookat=%d applied=%d pending=%d pos=(%.1f,%.1f,%.1f) "
              "focal=(%.1f,%.1f,%.1f) dist=%.1f near=%.2f far=%.1f lfar=%.1f "
              "gpu=%zu opaque=%zu bounds=%d",
              s_dump, has_lookat ? 1 : 0, s_lookat_applied ? 1 : 0,
              m_pending_scene_camera_focus ? 1 : 0, camera_position.x,
              camera_position.y, camera_position.z, focal.x, focal.y, focal.z,
              camera_distance, m_editor_camera->getNearClip(), cam_far,
              lighting_far,
              m_gpu_driven_draws.size(), m_opaque_mesh_draws.size(),
              sc != nullptr && sc->hasWorldBounds() ? 1 : 0);
          if (sc != nullptr && sc->hasWorldBounds()) {
            const AABB& bb = sc->getWorldBounds();
            std::fprintf(dump, " aabb=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)",
                         bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y,
                         bb.max.z);
          }
          if (!m_opaque_mesh_draws.empty()) {
            const Mat4& m = m_opaque_mesh_draws[0].model;
            const Vec3 t(m[3]);
            const float sx = glm::length(Vec3(m[0]));
            const float sy = glm::length(Vec3(m[1]));
            const float sz = glm::length(Vec3(m[2]));
            std::fprintf(dump, " model0 t=(%.2f,%.2f,%.2f) s=(%.5f,%.5f,%.5f)",
                         t.x, t.y, t.z, sx, sy, sz);
          }
          std::fprintf(dump, " env=%s\n",
                       has_lookat ? "BLUNDER_EDITOR_LOOKAT" : "null");
          std::fclose(dump);
        }
      }
    }
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
  phases.mark("cameraMs");

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
  frame_state.grid_plane = ForwardGridPlane::xy;
  frame_state.viewport_width = offscreen_extent.width;
  frame_state.viewport_height = offscreen_extent.height;
  if (m_viewport_layout_source != nullptr) {
    auto* slint_layout = static_cast<SlintSystem*>(m_viewport_layout_source);
    slint_layout->syncViewportProjectionMode(
        projection_mode == EditorCamera::ProjectionMode::perspective);
  }
  if (m_preview_settings_source != nullptr) {
    const BlinnPhongEditorSettings preview =
        m_preview_settings_source->previewSettings().get();
    frame_state.shading.light_direction = preview.light_direction;
    frame_state.shading.light_color = preview.light_color;
  }
  frame_state.shading.ssao_enabled = false;
  if (m_viewport_layout_source != nullptr) {
    auto* slint_layout = static_cast<SlintSystem*>(m_viewport_layout_source);
    frame_state.shading.froxel_occupancy_heatmap =
        slint_layout->froxelOccupancyHeatmapEnabled();
    frame_state.shading.vrs_rate_mask = slint_layout->vrsRateMaskEnabled();
    if (frame_state.shading.froxel_occupancy_heatmap && !m_deferred_path) {
      static bool s_logged_heatmap_without_deferred = false;
      if (!s_logged_heatmap_without_deferred) {
        s_logged_heatmap_without_deferred = true;
        LOG_WARN(
            "[RenderSystem] Froxel occupancy heatmap is on, but the Viewport "
            "is not on the Deferred path (BLUNDER_EDITOR_DEFERRED=0). Overlay "
            "will not draw.");
      }
    }
  }
  SceneInstance* active_scene = nullptr;
  if (g_runtime_global_context.m_scene_system != nullptr) {
    active_scene = g_runtime_global_context.m_scene_system->getActiveInstance();
  }
  frame_state.lighting_scene = active_scene;
  frame_state.live_scene_lighting = true;
  frame_state.shadow_caster_id = k_invalid_entity_id;
  glm::vec3 shadow_light_dir = glm::normalize(glm::vec3(0.45f, 0.7f, 0.55f));
  if (active_scene != nullptr) {
    frame_state.shadow_caster_id = pickDirectionalShadowCaster(*active_scene);
    if (isValid(frame_state.shadow_caster_id)) {
      const Vec3 emit =
          lightWorldEmit(active_scene->getWorldMatrix(frame_state.shadow_caster_id));
      // lookAt is -Z forward. Pass emit (sun onto the scene), not shading L,
      // or the ortho camera sits under the floor and the 1024 map stays empty.
      shadow_light_dir = emit;
    }
  }
  const bool host_is_player =
      g_runtime_global_context.hostMode() == EngineHostMode::Player;
  if (active_scene != nullptr) {
    frame_state.local_shadows = pickLocalShadowCasters(*active_scene);
  }
  frame_state.mesh_shadows = m_mesh_shadows.get();
  const bool has_shadow_casters =
      isValid(frame_state.local_shadows.directional) ||
      frame_state.local_shadows.point_count > 0 ||
      frame_state.local_shadows.spot_count > 0;
  frame_state.shadows_enabled =
      has_shadow_casters && (host_is_player || !editorShadowsForcedOff());

  glm::vec3 shadow_focus(0.0f);
  float shadow_ortho_half_extent = k_shadow_ortho_half_extent;
  float shadow_view_distance = 30.0f;
  float shadow_near_plane = k_shadow_near_plane;
  float shadow_far_plane = k_shadow_far_plane;
  if (active_scene != nullptr && active_scene->hasWorldBounds()) {
    const AABB& bounds = active_scene->getWorldBounds();
    shadow_focus = bounds.center();
    const DirectionalShadowPlacement place =
        computeDirectionalShadowPlacementFromAABB(bounds, shadow_light_dir);
    shadow_ortho_half_extent = place.ortho_half_extent;
    shadow_view_distance = place.view_distance;
    shadow_near_plane = place.near_plane;
    shadow_far_plane = place.far_plane;
  }

  computeDirectionalLightMatrices(
      shadow_light_dir, shadow_focus, shadow_ortho_half_extent,
      shadow_near_plane, shadow_far_plane, frame_state.light_view,
      frame_state.light_projection, frame_state.light_view_projection,
      shadow_view_distance);

  const bool viewport_target_changed =
      target_width != m_last_viewport_target_w ||
      target_height != m_last_viewport_target_h;
  const bool camera_changed =
      !matricesNearlyEqual(view, m_last_viewport_view) ||
      !matricesNearlyEqual(projection, m_last_viewport_projection);
  const bool scene_changed =
      m_viewport_render_generation != m_last_rendered_viewport_generation ||
      active_scene != m_last_rendered_scene_instance;
  frame_state.scene_static = !host_is_player && !scene_changed;
  const bool heatmap_changed = frame_state.shading.froxel_occupancy_heatmap !=
                               m_last_rendered_froxel_heatmap;
  const bool vrs_mask_changed =
      frame_state.shading.vrs_rate_mask != m_last_rendered_vrs_rate_mask;
  publishFrameTimingCounts(
      frame_state,
      static_cast<uint32_t>(m_gpu_driven_draws.size() + m_opaque_mesh_draws.size() +
                            m_transparent_mesh_draws.size()),
      static_cast<uint32_t>(m_gpu_driven_draws.size() + m_opaque_mesh_draws.size() +
                            m_transparent_mesh_draws.size()));
  // Programmatic LOOKAT / AABB snaps change the view without pointer
  // interaction. Skipping that record keeps presenting the origin-grid
  // frame while Unique gizmos sit on the courtyard.
  const bool skip_camera_only_zero_copy = false;
  // Player is a game view: idle skip is editor-only (static camera + generation).
  // Behaviour TRS and CPU skin still update the draw list; without a record the
  // last presented swapchain image stays frozen.
  if (!host_is_player && !m_force_viewport_render && !viewport_target_changed &&
      !camera_changed && !scene_changed && !heatmap_changed &&
      !vrs_mask_changed) {
    phases.flag("skip", 1);
    harvestReadyGpuTimestamps(vkSync(this));
    pollViewportPresent();
    return;
  }

  if (skip_camera_only_zero_copy) {
    phases.flag("skip", 2);
    pollViewportPresent();
    return;
  }

  // Two frames in flight and one of the two images is pinned as the Slint-bound
  // texture, so exactly one slot is recordable. Bind whatever the GPU finished
  // before claiming that slot: tryBeginRecordingSlot() gates on the same
  // slotReached() the presenter needs, so without this the recorder always wins
  // the race and overwrites the finished image before it is ever shown. Orbit
  // then submits at the GPU rate while the Viewport shows one frame per drag.
  pollViewportPresent();

  uint64_t bound_vk = 0;
  if (usesZeroCopyViewport() && m_viewport_layout_source != nullptr) {
    bound_vk = static_cast<SlintSystem*>(m_viewport_layout_source)
                   ->boundViewportVkImage();
    if (OffscreenRenderTarget* offscreen = vkOffscreenRt(this)) {
      const uint64_t write_vk = reinterpret_cast<uint64_t>(
          offscreen->getImage(m_current_frame));
      if (bound_vk != 0 && write_vk == bound_vk) {
        const uint32_t other =
            (m_current_frame + 1u) % VulkanSync::k_max_frames_in_flight;
        const uint64_t other_vk =
            reinterpret_cast<uint64_t>(offscreen->getImage(other));
        if (other_vk != bound_vk) {
          m_current_frame = other;
        }
      }
    }
  }

  if (!tryBeginRecordingSlot(m_current_frame)) {
    phases.flag("skip", 3);
    pollViewportPresent();
    if (m_viewport_render_generation != m_last_rendered_viewport_generation) {
      m_force_viewport_render = true;
    }
    return;
  }
  phases.mark("beginSlotMs");

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
        draw.entity_id = mesh_draw.entity_id;
        draw.gpu_bone_palette = mesh_draw.gpu_bone_palette;
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

  phases.mark("drawListMs");
  VkCommandBuffer command_buffer =
      m_mesh_pipeline->nativePipeline()->getCommandBuffer(m_current_frame);
  vkResetCommandBuffer(command_buffer, 0);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(command_buffer, &begin_info);
  if (FrameTimingService* timing = g_runtime_global_context.m_frame_timing.get()) {
    timing->beginGpuSlot(command_buffer, m_current_frame);
  }

  if (m_overlay_system) {
    m_overlay_system->begin_sync(frame_state, m_current_frame);
  }

  // Editor viewport shading + overlays + Copy Sink (ADR 0067 / 0068 / 0069).
  // Camera Preview below uses a dedicated DeferredRenderPath after execute.
  recordViewportGraph(
      command_buffer, frame_state, opaque_draws.data(),
      static_cast<uint32_t>(opaque_draws.size()), transparent_draws.data(),
      static_cast<uint32_t>(transparent_draws.size()), m_current_frame,
      host_is_player);
  if (m_deferred_path && m_viewport_layout_source != nullptr && !host_is_player) {
    static_cast<SlintSystem*>(m_viewport_layout_source)
        ->syncFroxelViewportStats(
            m_deferred_path->froxelDroppedLightAssignments(),
            m_deferred_path->froxelDroppedLightAssignmentsTotal());
  }

  uint32_t preview_width = 0;
  uint32_t preview_height = 0;
  const bool preview_recorded = recordCameraPreviewPass(
      command_buffer, frame_state, opaque_draws, transparent_draws, m_current_frame,
      preview_width, preview_height);

  if (FrameTimingService* timing = g_runtime_global_context.m_frame_timing.get()) {
    timing->collectTracy(command_buffer);
  }

  vkEndCommandBuffer(command_buffer);
  phases.mark("recordMs");

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  const uint64_t timeline_value =
      vkSync(this)->queueSubmit(vkCtx(this)->getGraphicsQueue(), submit_info);
  vkSync(this)->setSlotValue(m_current_frame, timeline_value);
  phases.mark("submitMs");

  if (preview_recorded) {
    m_camera_preview_readback_pending = true;
    m_camera_preview_readback_slot = m_current_frame;
    m_camera_preview_readback_w = preview_width;
    m_camera_preview_readback_h = preview_height;
  }

  const bool zero_copy_viewport = usesZeroCopyViewport();
  if (zero_copy_viewport) {
    notifyZeroCopySubmitted(m_current_frame, offscreen_extent.width,
                            offscreen_extent.height);
    pollViewportPresent();
  } else if (m_viewport_bridge) {
    m_viewport_bridge->notifyGpuSubmitted(m_current_frame, offscreen_extent.width,
                                          offscreen_extent.height);
    // Async readback: do NOT block on the timeline here. pollAndPresent() polls
    // the slot value to check if any previous frame's staging copy has completed.
    pollViewportPresent();
  }
  phases.mark("presentPollMs");

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
  m_last_rendered_froxel_heatmap = frame_state.shading.froxel_occupancy_heatmap;
  m_last_rendered_vrs_rate_mask = frame_state.shading.vrs_rate_mask;
  m_last_rendered_scene_instance = active_scene;
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

  const bool overlays =
      editorOverlaysEnabled(g_runtime_global_context.hostMode());
  const bool authorship =
      playerAuthorshipInputEnabled(g_runtime_global_context.hostMode());

  if (overlays && m_overlay_system && m_editor_camera) {
    auto& transform_ctrl = m_overlay_system->transform_gizmo().controller();
    auto& camera_ctrl = m_overlay_system->camera_gizmo().controller();
    const bool scene_gizmos = m_overlay_system->sceneGizmosVisible();
    const EventType type = event.getEventType();
    const bool mouse_event =
        type == EventType::MouseMoved || type == EventType::MouseButtonPressed ||
        type == EventType::MouseButtonReleased;
    if (scene_gizmos || transform_ctrl.isDragging() || !mouse_event) {
      transform_ctrl.onEvent(event, *m_editor_camera);
      if (event.handled) {
        return;
      }
    }
    if (scene_gizmos || camera_ctrl.isDragging()) {
      camera_ctrl.onEvent(event, *m_editor_camera);
      if (event.handled) {
        return;
      }
    }
  }

  if (overlays && m_overlay_system && m_editor_camera &&
      event.getEventType() == EventType::MouseMoved && !event.handled) {
    auto& mouse_event = static_cast<MouseMovedEvent&>(event);
    {
      auto& gizmo_ctrl = m_overlay_system->transform_gizmo().controller();
      const Vec2 pos(mouse_event.getX(), mouse_event.getY());
      bool hover_changed = false;
      if (m_overlay_system->sceneGizmosVisible() &&
          gizmo_ctrl.getMode() != TransformGizmoMode::none &&
          !gizmo_ctrl.isDragging()) {
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

  if (overlays && m_overlay_system && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    auto& mouse_event = static_cast<MouseButtonPressedEvent&>(event);
    if (mouse_event.getMouseButton() == SDL_BUTTON_LEFT &&
        mouse_event.hasMousePosition()) {
      if (m_overlay_system->navigate_gizmo().tryHandleMouseClick(
              Vec2(mouse_event.getX(), mouse_event.getY()), *m_editor_camera)) {
        event.handled = true;
        return;
      }
      if (m_overlay_system->tryHandleCameraOrLightGizmoClick(
              Vec2(mouse_event.getX(), mouse_event.getY()), *m_editor_camera)) {
        event.handled = true;
        return;
      }
    }
  }

  if (authorship && !event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    auto& mouse_event = static_cast<MouseButtonPressedEvent&>(event);
    if (mouse_event.hasMousePosition() &&
        isViewportPickInputPoint(*m_editor_camera, mouse_event.getX(),
                                 mouse_event.getY()) &&
        g_runtime_global_context.m_viewport_pick) {
      if (mouse_event.getMouseButton() == SDL_BUTTON_RIGHT ||
          mouse_event.getMouseButton() == SDL_BUTTON_MIDDLE) {
        g_runtime_global_context.m_viewport_pick->onCameraInteractionStarted();
      }
    }
  }

  if (authorship && !event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseMoved) {
    auto& mouse_event = static_cast<MouseMovedEvent&>(event);
    if (g_runtime_global_context.m_viewport_pick &&
        isViewportPickInputPoint(*m_editor_camera, mouse_event.getX(),
                                 mouse_event.getY())) {
      g_runtime_global_context.m_viewport_pick->onViewportPointerMoved(
          mouse_event.getX(), mouse_event.getY());
    }
  }

  if (authorship && !event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    auto& mouse_event = static_cast<MouseButtonPressedEvent&>(event);
    if (mouse_event.getMouseButton() == SDL_BUTTON_RIGHT &&
        mouse_event.hasMousePosition() &&
        isViewportPickInputPoint(*m_editor_camera, mouse_event.getX(),
                                 mouse_event.getY()) &&
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

  if (authorship && !event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonPressed) {
    auto& mouse_event = static_cast<MouseButtonPressedEvent&>(event);
    if (mouse_event.getMouseButton() == SDL_BUTTON_LEFT &&
        mouse_event.hasMousePosition() &&
        isViewportPickInputPoint(*m_editor_camera, mouse_event.getX(),
                                 mouse_event.getY()) &&
        g_runtime_global_context.m_viewport_pick) {
      g_runtime_global_context.m_viewport_pick->onViewportLeftPressed(
          mouse_event.getX(), mouse_event.getY());
    }
  }

  if (authorship && !event.handled && m_editor_camera &&
      event.getEventType() == EventType::MouseButtonReleased) {
    auto& mouse_event = static_cast<MouseButtonReleasedEvent&>(event);
    if (mouse_event.getMouseButton() == SDL_BUTTON_LEFT &&
        mouse_event.hasMousePosition() &&
        g_runtime_global_context.m_viewport_pick) {
      const uint16_t modifiers =
          static_cast<uint16_t>(SDL_GetModState() & 0xFFFFu);
      if (isViewportPickInputPoint(*m_editor_camera, mouse_event.getX(),
                                   mouse_event.getY())) {
        g_runtime_global_context.m_viewport_pick->onViewportLeftReleased(
            mouse_event.getX(), mouse_event.getY(), modifiers);
        event.handled = true;
        return;
      }
    }
  }

  if (overlays && m_editor_camera && !isTranslateModalSessionActive() &&
      !event.handled && event.getEventType() == EventType::KeyPressed) {
    auto& key_event = static_cast<KeyPressedEvent&>(event);
    if (!key_event.isRepeat()) {
      const bool align_view = isAlignViewToCameraShortcut(key_event);
      const bool align_camera = isAlignCameraToViewShortcut(key_event);
      if (align_view || align_camera) {
        SceneInstance* scene =
            g_runtime_global_context.m_scene_system != nullptr
                ? g_runtime_global_context.m_scene_system->getActiveInstance()
                : nullptr;
        eastl::vector<EntityId> selected_ids;
        if (g_runtime_global_context.m_editor_selection) {
          selected_ids =
              g_runtime_global_context.m_editor_selection->getSelectedIds();
        }
        bool applied = false;
        const eastl::span<const EntityId> selection_span(
            selected_ids.data(), selected_ids.size());
        if (align_camera) {
          applied = alignCameraToView(scene, *m_editor_camera, selection_span);
        } else if (align_view && scene != nullptr) {
          applied = alignViewToCamera(*m_editor_camera, *scene, selection_span);
        }
        if (applied) {
          requestViewportRedraw();
          if (g_runtime_global_context.m_slint_system) {
            g_runtime_global_context.m_slint_system->syncInspectorFromSelection();
          }
          event.handled = true;
          return;
        }
      }
    }
  }

  if (authorship && m_editor_camera) {
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

bool RenderSystem::areSceneGizmosVisible() const {
  if (m_overlay_system) {
    return m_overlay_system->sceneGizmosVisible();
  }
  return true;
}

void RenderSystem::toggleSceneGizmosVisible() {
  if (!m_overlay_system) {
    return;
  }
  m_overlay_system->setSceneGizmosVisible(!m_overlay_system->sceneGizmosVisible());
  requestViewportRedraw();
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->syncTransformToolbarFromEngine();
  }
}

}  // namespace Blunder
