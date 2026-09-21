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
#include "runtime/function/render/gpu_driven/gpu_driven_types.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
#include "runtime/function/render/opaque_mesh_draw.h"

#include "runtime/function/render/vulkan/vulkan_sync.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class Event;
class AssetManager;
class DeferredRenderPath;
class EditorCamera;
class ForwardRenderPath;
class GpuDrivenRenderer;
class OverlaySystem;
class SceneInstance;
class SsaOPass;
class VolumetricFogPass;
class GpuMesh;
class MeshShadowSystem;
class RenderDocCapture;
class ShadowMapTarget;
class MaterialAsset;
class MeshAsset;
class Texture2DAsset;
class TextureLoader;
class MeshLoader;
class VulkanBuffer;
class VulkanPipeline;
class VulkanTexture;
class VulkanSync;
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
struct ForwardOpaqueDraw;
struct ForwardFrameState;

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
///   1. assembles a forward draw list and records the viewport Scene plus
///      overlay/copy Passes through Frame graph execute (Copy is the Sink),
///   2. submits the command buffer signaling a timeline value and polls
///      previous slots asynchronously (no blocking stall), and
///   3. pushes the latest completed RGBA8 pixels into the viewport presenter.
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
  uint32_t textureUploadInFlightCount() const;
  /// Scene drop: in-flight Texture Loader completions must not write dropped images.
  void dropInFlightTextures();
  /// Scene drop: in-flight Mesh Loader completions must not bind dropped meshes.
  void dropInFlightMeshes();
  void pumpMeshLoader(SceneInstance* scene_instance);
  GpuMesh* getOrUploadGpuMesh(const MeshAsset* mesh_asset);
  GpuMesh* getOrUploadGpuMeshByKey(const eastl::string& cache_key,
                                   const void* vertex_bytes,
                                   size_t vertex_byte_size, const uint32_t* indices,
                                   size_t index_count);
  GpuMesh* findUploadedGpuMesh(const eastl::string& cache_key) const;
  GpuMesh* updateOrUploadSkinnedGpuMesh(const eastl::string& base_cache_key,
                                        const void* vertex_bytes,
                                        size_t vertex_byte_size,
                                        const uint32_t* indices,
                                        size_t index_count);
  /// CPU-skinned `#skinned` GPU mesh when present (same vertex layout as bind).
  GpuMesh* gpuMeshForEditorOverlay(const MeshAsset* mesh_asset);

  /// Drop all uploaded GPU meshes (Editor Asset Hot Reload after Mesh Reimport).
  void invalidateAllGpuMeshes() { clearGpuMeshes(); }

  bool addOpaqueMeshDraw(
      GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
      VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
      VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
      const glm::mat4& model, float alpha_cutoff = 0.5f,
      cgltf_alpha_mode alpha_mode = cgltf_alpha_mode_opaque, bool double_sided = false,
      eastl::vector<glm::mat4> gpu_bone_palette = {},
      EntityId entity_id = k_invalid_entity_id);
  bool addTransparentMeshDraw(
      GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
      VulkanTexture* base_color_texture, VulkanTexture* metallic_roughness_texture,
      VulkanTexture* normal_texture, VulkanTexture* occlusion_texture,
      const glm::mat4& model, float alpha_cutoff = 0.5f, bool double_sided = false,
      eastl::vector<glm::mat4> gpu_bone_palette = {},
      EntityId entity_id = k_invalid_entity_id);
  bool addGpuDrivenDraw(GpuMesh* gpu_mesh, eastl::shared_ptr<MaterialAsset> material,
                        VulkanTexture* base_color_texture,
                        VulkanTexture* metallic_roughness_texture,
                        VulkanTexture* normal_texture,
                        VulkanTexture* occlusion_texture, const glm::mat4& model,
                        float alpha_cutoff, cgltf_alpha_mode alpha_mode,
                        bool double_sided, EntityId entity_id);
  void clearOpaqueMeshDraws();
  void clearTransparentMeshDraws();
  void clearGpuDrivenDraws();
  VulkanTexture* getFallbackTexture() const { return m_fallback_texture; }

  EditorCamera* getEditorCamera() const { return m_editor_camera.get(); }
  uint64_t getViewportRenderGeneration() const {
    return m_viewport_render_generation;
  }
  EntityId pickEntityAtWindowPosition(float window_x, float window_y);
  eastl::vector<EntityId> pickAllEntitiesAtWindowPosition(float window_x,
                                                         float window_y);

  /// Frames the active scene once the viewport has a valid size (see tick).
  void requestSceneCameraFocus();
  /// Drop previous-frame GPU-driven Hi-Z after an editor camera snap / look-at
  /// so a courtyard close-up cannot false-occlude the zoomed-out mesh.
  void invalidateGpuDrivenOcclusion();

  /// Live document changed: force a Viewport record (idle skip would keep the
  /// previous offscreen) and snap the editor camera to the new scene bounds.
  void notifyActiveSceneChanged();

  void setTransformGizmoMode(TransformGizmoMode mode);
  TransformGizmoMode getTransformGizmoMode() const;
  void toggleTransformGizmoSpace();
  bool isTransformGizmoSpaceGlobal() const;
  bool isTransformGizmoDragging() const;
  bool isTranslateModalSessionActive() const;
  bool areSceneGizmosVisible() const;
  void toggleSceneGizmosVisible();
  /// Forces the next viewport offscreen pass (gizmo mode/space, overlays, etc.).
  void requestViewportRedraw();
  rhi::IRenderBackend* getRenderBackend() const { return m_backend.get(); }
  rhi::IOffscreenRenderTarget* getOffscreenTarget() const {
    return m_offscreen.get();
  }
  /// CPU readback of the current Play-rule / viewport color target.
  bool readbackOffscreenRgba(eastl::vector<uint8_t>& out_rgba, uint32_t& out_width,
                             uint32_t& out_height);
  bool isVulkanBackend() const;
  OverlaySystem* getOverlaySystem() const { return m_overlay_system.get(); }
  bool viewportUsesDeferredPath() const { return m_deferred_path != nullptr; }
  bool vrsAttachmentEnabled() const;
  bool fragmentShadingRateExtensionEnabled() const;
  void vrsTexelSize(uint32_t* width, uint32_t* height) const;
  eastl::string physicalDeviceName() const;

 private:
  void initializeVulkanPath(const RenderSystemInitInfo& info);
  void initializeD3D12SkeletonPath(const RenderSystemInitInfo& info);
  void initializeTextureLoader();
  void tickVulkan(float delta_time, uint32_t target_width,
                  uint32_t target_height);
  void recordViewportGraph(VkCommandBuffer command_buffer,
                            const ForwardFrameState& frame_state,
                            const ForwardOpaqueDraw* opaque_draws,
                            uint32_t opaque_draw_count,
                            const ForwardOpaqueDraw* transparent_draws,
                            uint32_t transparent_draw_count,
                            uint32_t frame_index, bool host_is_player);
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
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_skinned_mesh_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_skinned_transparent_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_shadow_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_skinned_shadow_pipeline;
  eastl::unique_ptr<OverlaySystem> m_overlay_system;
  eastl::unique_ptr<ShadowMapTarget> m_shadow_map;
  eastl::unique_ptr<MeshShadowSystem> m_mesh_shadows;
  eastl::unique_ptr<EditorCamera> m_editor_camera;
  eastl::unique_ptr<RenderDocCapture> m_renderdoc_capture;
  eastl::unique_ptr<ForwardRenderPath> m_forward_path;
  /// Editor Viewport unless `BLUNDER_EDITOR_DEFERRED=0`. Player always.
  /// Camera Preview / Mesh Preview / Thumbnail own separate paths.
  eastl::unique_ptr<DeferredRenderPath> m_deferred_path;
  eastl::unique_ptr<SsaOPass> m_ssao_pass;
  eastl::unique_ptr<VolumetricFogPass> m_volumetric_fog_pass;
  eastl::unique_ptr<GpuDrivenRenderer> m_gpu_driven_renderer;
  eastl::unique_ptr<TextureLoader> m_texture_loader;

  eastl::unordered_map<eastl::string, eastl::unique_ptr<GpuMesh>> m_gpu_meshes;
  VulkanTexture* m_fallback_texture{nullptr};
  eastl::vector<OpaqueMeshDraw> m_opaque_mesh_draws;
  eastl::vector<OpaqueMeshDraw> m_transparent_mesh_draws;
  eastl::vector<GpuDrivenDraw> m_gpu_driven_draws;
  eastl::shared_ptr<MaterialAsset> m_inspector_material;
  uint32_t m_current_frame{0};
  bool m_pending_scene_camera_focus{false};
  bool m_refocus_when_mesh_draws_ready{false};

  uint32_t m_deferred_rt_width{0};
  uint32_t m_deferred_rt_height{0};
  uint32_t m_deferred_rt_stable_frames{0};

  glm::mat4 m_last_viewport_view{1.0f};
  glm::mat4 m_last_viewport_projection{1.0f};
  uint32_t m_last_viewport_target_w{0};
  uint32_t m_last_viewport_target_h{0};
  uint32_t m_viewport_render_generation{0};
  uint32_t m_last_rendered_viewport_generation{0};
  bool m_last_rendered_froxel_heatmap{false};
  bool m_last_rendered_vrs_rate_mask{false};
  bool m_force_viewport_render{true};
  SceneInstance* m_last_rendered_scene_instance{nullptr};
  bool m_defer_viewport_for_texture_residency{false};

  struct ZeroCopyPresentSlot {
    uint32_t width{0};
    uint32_t height{0};
    bool pending_gpu{false};
    uint64_t completed_generation{0};
    uint64_t submit_ns{0};
  };
  eastl::array<ZeroCopyPresentSlot, VulkanSync::k_max_frames_in_flight>
      m_zero_copy_slots{};
  uint64_t m_zero_copy_last_presented_generation{0};
  uint64_t m_zero_copy_next_generation{1};

  bool usesZeroCopyViewport() const;
  void resetZeroCopyPresentState();
  void notifyZeroCopySubmitted(uint32_t slot, uint32_t width, uint32_t height);
  void pollZeroCopyAndPresent();
  bool tryBeginRecordingSlot(uint32_t slot);
  void pollViewportPresent();

  void markViewportRenderDirty();
  void pollViewportPickIfActive();

  void shutdownCameraPreviewResources();
  void ensureCameraPreviewOffscreen(uint32_t width, uint32_t height);
  void ensureCameraPreviewOffscreenIfNeeded();
  void resizeCameraPreviewReadback(uint32_t width, uint32_t height);
  void clearCameraPreviewPresentation();
  void syncCameraPreviewSkipClear();
  bool recordCameraPreviewPass(
      VkCommandBuffer command_buffer, const ForwardFrameState& main_frame_state,
      const eastl::vector<ForwardOpaqueDraw>& opaque_draws,
      const eastl::vector<ForwardOpaqueDraw>& transparent_draws,
      uint32_t frame_index, uint32_t& out_width, uint32_t& out_height);
  void tryPresentCameraPreview();

  eastl::unique_ptr<rhi::IOffscreenRenderTarget> m_camera_preview_offscreen;
  eastl::unique_ptr<DeferredRenderPath> m_camera_preview_deferred;
  eastl::unique_ptr<VulkanBuffer> m_camera_preview_staging;
  void* m_camera_preview_staging_map{nullptr};
  uint32_t m_camera_preview_staging_w{0};
  uint32_t m_camera_preview_staging_h{0};
  bool m_camera_preview_readback_pending{false};
  uint32_t m_camera_preview_readback_slot{0};
  uint32_t m_camera_preview_readback_w{0};
  uint32_t m_camera_preview_readback_h{0};
  bool m_camera_preview_image_cleared{true};

  eastl::unique_ptr<VulkanBuffer> m_play_frame_staging;
  uint32_t m_play_frame_staging_w{0};
  uint32_t m_play_frame_staging_h{0};
};

}  // namespace Blunder
