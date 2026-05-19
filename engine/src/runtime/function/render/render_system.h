#pragma once

#include <cstdint>

#include "EASTL/array.h"
#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"
#include <glm/vec3.hpp>

#include "runtime/function/render/forward/forward_frame_state.h"

namespace Blunder {

class Event;
class AssetManager;
class EditorCamera;
class ForwardRenderPath;
class RenderDocCapture;
class MaterialAsset;
class Texture2DAsset;
class VulkanBuffer;
class VulkanPipeline;
class VulkanTexture;
class WindowSystem;
class SlintSystem;

namespace rhi {
class IGraphicsPipeline;
class IOffscreenRenderTarget;
class IRenderBackend;
class IViewportPresenter;
}  // namespace rhi

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}

struct ForwardOpaqueDrawSource;
struct Vertex;

struct RenderSystemInitInfo {
  AssetManager* asset_manager{nullptr};
  WindowSystem* window_system{nullptr};
  bool enable_validation{true};
  rhi::IViewportPresenter* viewport_presenter{nullptr};
  /// Used for editor-camera viewport rect queries (layout from Slint).
  SlintSystem* viewport_layout_source{nullptr};
};

/// Runtime renderer.
///
/// The engine no longer owns a window swapchain. Slint's Skia renderer is in
/// charge of presenting to the HWND. Each frame this system:
///   1. assembles a forward draw list and delegates scene recording to
///      `ForwardRenderPath`,
///   2. transitions the off-screen color image and copies it into a host-visible
///      staging buffer,
///   3. waits for the GPU to finish (single-frame stall, suitable for an
///      editor preview), and
///   4. pushes the resulting RGBA8 pixels into the viewport presenter.
class RenderSystem final {
 public:
  RenderSystem();
  ~RenderSystem();

  void initialize(const RenderSystemInitInfo& info);
  void shutdown();

  void tick(float delta_time, uint32_t target_width, uint32_t target_height);
  void onEvent(Event& event);

  VulkanTexture* ensureTextureUploaded(const Texture2DAsset* texture_asset);

  EditorCamera* getEditorCamera() const { return m_editor_camera.get(); }
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
  void recreateReadbackStaging(uint32_t width, uint32_t height);
  bool createOpaqueDrawSourceFromMesh(const void* vertex_bytes,
                                      VkDeviceSize vertex_byte_size,
                                      const uint32_t* indices,
                                      size_t index_count,
                                      eastl::shared_ptr<MaterialAsset> material,
                                      VulkanTexture* base_color_texture,
                                      uint32_t slot_index,
                                      const glm::vec3& translation,
                                      const glm::vec3& scale, bool animate_spin);

  AssetManager* m_asset_manager{nullptr};
  WindowSystem* m_window_system{nullptr};
  rhi::IViewportPresenter* m_viewport_presenter{nullptr};
  SlintSystem* m_viewport_layout_source{nullptr};
  eastl::unique_ptr<rhi::IRenderBackend> m_backend;

  eastl::unique_ptr<rhi::IOffscreenRenderTarget> m_offscreen;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_mesh_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_grid_pipeline;
  eastl::unique_ptr<EditorCamera> m_editor_camera;
  eastl::unique_ptr<RenderDocCapture> m_renderdoc_capture;
  eastl::unique_ptr<ForwardRenderPath> m_forward_path;

  eastl::unordered_map<eastl::string, eastl::unique_ptr<VulkanTexture>>
      m_uploaded_textures;
  VulkanTexture* m_fallback_texture{nullptr};
  eastl::vector<ForwardOpaqueDrawSource> m_opaque_draw_sources;
  eastl::shared_ptr<MaterialAsset> m_inspector_material;
  float m_primary_mesh_rotation_radians{0.0f};
  uint32_t m_current_frame{0};
  ForwardGridPlane m_grid_plane{ForwardGridPlane::xy};

  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_readback_staging;
  uint32_t m_readback_width{0};
  uint32_t m_readback_height{0};
  eastl::vector<uint8_t> m_readback_pixels;
};

}  // namespace Blunder
