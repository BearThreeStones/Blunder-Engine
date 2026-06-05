#pragma once

#include <cstdint>

#include "EASTL/array.h"
#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"
#include <glm/vec3.hpp>

#include <cgltf.h>

#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
#include "runtime/function/render/opaque_mesh_draw.h"

namespace Blunder {

class Event;
class AssetManager;
class EditorCamera;
class ForwardRenderPath;
class OverlaySystem;
class SsaOPass;
class GpuMesh;
class RenderDocCapture;
class ShadowMapTarget;
class MaterialAsset;
class MeshAsset;
class Texture2DAsset;
class VulkanBuffer;
class VulkanPipeline;
class VulkanTexture;
class WindowSystem;
class SlintSystem;
class UiHost;
class UIViewportBridge;
class IViewportSink;

namespace rhi {
class IGraphicsPipeline;
class IOffscreenRenderTarget;
class IRenderBackend;
}  // namespace rhi

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}

struct Vertex;

struct RenderSystemInitInfo {
  AssetManager* asset_manager{nullptr};
  WindowSystem* window_system{nullptr};
  bool enable_validation{true};
  /// Used for editor-camera viewport rect queries (layout from Slint).
  SlintSystem* viewport_layout_source{nullptr};
  /// Live Blinn-Phong / SSAO preview parameters (C++ SSOT via UiHost).
  UiHost* preview_settings_source{nullptr};
  UIViewportBridge* viewport_bridge{nullptr};
  IViewportSink* viewport_sink{nullptr};
};

/// Raw engine Vulkan handles for the Slint shared-device renderer. Handles are
/// reinterpret_cast'd to uint64_t so the UI layer (and the Slint C FFI) need not
/// depend on Vulkan headers. `valid` is false on the D3D12 skeleton backend.
struct SharedVulkanHandles {
  uint64_t instance{0};
  uint64_t physical_device{0};
  uint64_t device{0};
  uint32_t graphics_queue_family{0};
  bool valid{false};
};

/// Runtime renderer.
///
/// The engine no longer owns a window swapchain. Slint's Skia renderer is in
/// charge of presenting to the HWND. Each frame this system:
///   1. assembles a forward draw list and delegates scene recording to
///      `ForwardRenderPath`,
///   2. transitions the off-screen color image and copies it into a host-visible
///      staging buffer,
///   3. submits the command buffer with a fence and polls previous frames'
///      fences asynchronously (no blocking stall), and
///   4. pushes the latest completed RGBA8 pixels into the viewport presenter.
class RenderSystem final {
 public:
  RenderSystem();
  ~RenderSystem();

  void initialize(const RenderSystemInitInfo& info);
  void shutdown();

  /// Creates only the GPU backend (Vulkan device/allocator/sync). Lets the
  /// engine's Vulkan device exist before the Slint renderer is created, so the
  /// UI can adopt it (shared-device / zero-copy viewport). Idempotent; a later
  /// initialize() reuses the backend created here.
  void initializeBackend(const RenderSystemInitInfo& info);

  /// Engine Vulkan handles for the Slint shared-device renderer. Valid only
  /// after initializeBackend()/initialize() on the Vulkan backend.
  SharedVulkanHandles getSharedVulkanHandles() const;

  void tick(float delta_time, uint32_t target_width, uint32_t target_height);
  void onEvent(Event& event);

  VulkanTexture* ensureTextureUploaded(const Texture2DAsset* texture_asset);
  GpuMesh* getOrUploadGpuMesh(const MeshAsset* mesh_asset);
  GpuMesh* getOrUploadGpuMeshByKey(const eastl::string& cache_key,
                                   const void* vertex_bytes,
                                   size_t vertex_byte_size, const uint32_t* indices,
                                   size_t index_count);

  bool addOpaqueMeshDraw(
      GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
      VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
      VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
      const glm::mat4& model, float alpha_cutoff = 0.5f,
      cgltf_alpha_mode alpha_mode = cgltf_alpha_mode_opaque, bool double_sided = false);
  bool addTransparentMeshDraw(
      GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
      VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
      VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
      const glm::mat4& model, float alpha_cutoff = 0.5f, bool double_sided = false);
  void clearOpaqueMeshDraws();
  void clearTransparentMeshDraws();
  VulkanTexture* getFallbackTexture() const { return m_fallback_texture; }

  EditorCamera* getEditorCamera() const { return m_editor_camera.get(); }

  /// Frames the active scene once the viewport has a valid size (see tick).
  void requestSceneCameraFocus();

  void setTransformGizmoMode(TransformGizmoMode mode);
  TransformGizmoMode getTransformGizmoMode() const;
  void toggleTransformGizmoSpace();
  bool isTransformGizmoSpaceGlobal() const;
  rhi::IRenderBackend* getRenderBackend() const { return m_backend.get(); }
  rhi::IOffscreenRenderTarget* getOffscreenTarget() const {
    return m_offscreen.get();
  }
  bool isVulkanBackend() const;

 private:
  void initializeVulkanPath(const RenderSystemInitInfo& info);
  void initializeD3D12SkeletonPath(const RenderSystemInitInfo& info);
  void tickVulkan(float delta_time, uint32_t target_width,
                  uint32_t target_height);
  void tickD3D12Skeleton(float delta_time, uint32_t target_width,
                         uint32_t target_height);

  void resizeOffscreenIfNeeded(uint32_t width, uint32_t height);
  void applyDeferredOffscreenResize();
  /// Applies pending offscreen resize immediately when Slint target size is stable.
  void flushOffscreenResizeToTarget(uint32_t target_width, uint32_t target_height);
  void resizeViewportReadback(uint32_t width, uint32_t height);
  void clearGpuMeshes();

  AssetManager* m_asset_manager{nullptr};
  WindowSystem* m_window_system{nullptr};
  SlintSystem* m_viewport_layout_source{nullptr};
  UiHost* m_preview_settings_source{nullptr};
  UIViewportBridge* m_viewport_bridge{nullptr};
  IViewportSink* m_viewport_sink{nullptr};
  eastl::unique_ptr<rhi::IRenderBackend> m_backend;

  eastl::unique_ptr<rhi::IOffscreenRenderTarget> m_offscreen;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_mesh_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_transparent_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_shadow_pipeline;
  eastl::unique_ptr<OverlaySystem> m_overlay_system;
  eastl::unique_ptr<ShadowMapTarget> m_shadow_map;
  eastl::unique_ptr<EditorCamera> m_editor_camera;
  eastl::unique_ptr<RenderDocCapture> m_renderdoc_capture;
  eastl::unique_ptr<ForwardRenderPath> m_forward_path;
  eastl::unique_ptr<SsaOPass> m_ssao_pass;

  eastl::unordered_map<eastl::string, eastl::unique_ptr<VulkanTexture>>
      m_uploaded_textures;
  eastl::unordered_map<eastl::string, eastl::unique_ptr<GpuMesh>> m_gpu_meshes;
  VulkanTexture* m_fallback_texture{nullptr};
  eastl::vector<OpaqueMeshDraw> m_opaque_mesh_draws;
  eastl::vector<OpaqueMeshDraw> m_transparent_mesh_draws;
  eastl::shared_ptr<MaterialAsset> m_inspector_material;
  uint32_t m_current_frame{0};
  bool m_pending_scene_camera_focus{false};
  bool m_refocus_when_mesh_draws_ready{false};
  ForwardGridPlane m_grid_plane{ForwardGridPlane::xy};

  uint32_t m_deferred_rt_width{0};
  uint32_t m_deferred_rt_height{0};
  uint32_t m_deferred_rt_stable_frames{0};

  glm::mat4 m_last_viewport_view{1.0f};
  glm::mat4 m_last_viewport_projection{1.0f};
  uint32_t m_last_viewport_target_w{0};
  uint32_t m_last_viewport_target_h{0};
  uint32_t m_viewport_render_generation{0};
  uint32_t m_last_rendered_viewport_generation{0};
  bool m_force_viewport_render{true};

  void markViewportRenderDirty();
};

}  // namespace Blunder
