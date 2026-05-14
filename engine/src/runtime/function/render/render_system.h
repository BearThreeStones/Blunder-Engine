#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "EASTL/array.h"
#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

namespace Blunder {

class Event;
class AssetManager;
class RenderDocCapture;
class SlangCompiler;
class EditorCamera;
class OffscreenRenderTarget;
class SlintSystem;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class VulkanPipeline;
class VulkanSync;
class VulkanTexture;
class WindowSystem;
class Texture2DAsset;

struct Vertex;

struct RenderSystemInitInfo {
  AssetManager* asset_manager{nullptr};
  WindowSystem* window_system{nullptr};
  bool enable_validation{true};
};

/// Runtime renderer.
///
/// The engine no longer owns a window swapchain. Slint's Skia renderer is in
/// charge of presenting to the HWND. Each frame this system:
///   1. renders the 3D scene to an off-screen `OffscreenRenderTarget`,
///   2. transitions the resulting image to TRANSFER_SRC and copies it into
///      a host-visible staging buffer,
///   3. waits for the GPU to finish (single-frame stall, suitable for an
///      editor preview), and
///   4. pushes the resulting RGBA8 pixels into `SlintSystem` so the Slint
///      `Image` control in the central viewport repaints with the latest
///      contents.
class RenderSystem final {
 public:
  RenderSystem();
  ~RenderSystem();

  void initialize(const RenderSystemInitInfo& info);
  void shutdown();

  /// Renders one frame.
  /// @param delta_time   Elapsed seconds since the last call.
  /// @param target_width Desired viewport width in pixels (from Slint).
  /// @param target_height Desired viewport height in pixels (from Slint).
  /// @param slint_system Receives the readback pixels via setViewportImage.
  void tick(float delta_time, uint32_t target_width, uint32_t target_height,
            SlintSystem* slint_system);
  void onEvent(Event& event);
  VulkanTexture* ensureTextureUploaded(const Texture2DAsset* texture_asset);

  VulkanContext* getVulkanContext() const { return m_context.get(); }
  VulkanAllocator* getAllocator() const { return m_allocator.get(); }
  EditorCamera* getEditorCamera() const { return m_editor_camera.get(); }
  OffscreenRenderTarget* getOffscreenRenderTarget() const {
    return m_offscreen_rt.get();
  }

 private:
  enum class GridPlane : uint32_t {
    xy = 0,
    xz = 1,
    yz = 2,
  };

  void resizeOffscreenIfNeeded(uint32_t width, uint32_t height);
  void recreateReadbackStaging(uint32_t width, uint32_t height);

  AssetManager* m_asset_manager{nullptr};
  WindowSystem* m_window_system{nullptr};
  eastl::shared_ptr<SlangCompiler> m_slang_compiler;
  eastl::shared_ptr<VulkanContext> m_context;
  eastl::shared_ptr<VulkanAllocator> m_allocator;
  eastl::shared_ptr<VulkanSync> m_sync;
  eastl::shared_ptr<VulkanPipeline> m_mesh_pipeline;
  eastl::shared_ptr<VulkanPipeline> m_grid_pipeline;
  eastl::unique_ptr<OffscreenRenderTarget> m_offscreen_rt;
  eastl::unique_ptr<EditorCamera> m_editor_camera;
  // RenderDoc In-Application API bridge. Headless rendering means RenderDoc
  // cannot detect frame boundaries automatically, so we drive
  // Start/EndFrameCapture explicitly around each tick. F11 (KeyPressedEvent)
  // schedules a single-frame capture.
  eastl::unique_ptr<RenderDocCapture> m_renderdoc_capture;
  eastl::unordered_map<eastl::string, eastl::unique_ptr<VulkanTexture>>
      m_uploaded_textures;
  eastl::unique_ptr<VulkanBuffer> m_demo_mesh_vertex_buffer;
  eastl::unique_ptr<VulkanBuffer> m_demo_mesh_index_buffer;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_mesh_uniform_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_grid_uniform_buffers;
  VkDescriptorPool m_mesh_descriptor_pool{VK_NULL_HANDLE};
  eastl::vector<VkDescriptorSet> m_mesh_descriptor_sets;
  VkDescriptorPool m_grid_descriptor_pool{VK_NULL_HANDLE};
  eastl::vector<VkDescriptorSet> m_grid_descriptor_sets;
  uint32_t m_demo_mesh_index_count{0};
  float m_demo_mesh_rotation_radians{0.0f};
  uint32_t m_current_frame{0};
  GridPlane m_grid_plane{GridPlane::xy};

  // CPU readback staging buffer (one per in-flight frame). Kept as
  // host-visible host-cached so it can be mapped and read directly.
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_readback_staging;
  uint32_t m_readback_width{0};
  uint32_t m_readback_height{0};
  // Linear scratch buffer used to copy/swizzle staging memory before
  // handing it to Slint. R8G8B8A8_UNORM offscreen format already matches
  // Slint::Rgba8Pixel, so this is a straight copy.
  eastl::vector<uint8_t> m_readback_pixels;
};

}  // namespace Blunder
