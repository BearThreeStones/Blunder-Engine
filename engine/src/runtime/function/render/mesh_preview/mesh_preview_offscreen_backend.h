#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "runtime/function/render/mesh_preview/mesh_preview_render.h"
#include "runtime/function/render/mesh_preview/mesh_preview_draw_builder.h"
#include "runtime/function/render/preview_render_target_owner.h"
#include "runtime/function/render/scene_thumbnail/i_scene_still_gpu.h"

namespace Blunder {

class AssetManager;
class ForwardRenderPath;
class GpuMesh;
class Texture2DAsset;
class VulkanBuffer;
class VulkanTexture;

namespace rhi {
class IOffscreenRenderTarget;
class IRenderBackend;
}  // namespace rhi

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}

class SceneInstance;

/// Owns the dedicated Mesh Preview RT, forward draw path, and CPU readback.
/// Separate from RenderSystem's Camera Preview and main viewport targets.
class MeshPreviewOffscreenBackend final : public IMeshPreviewRenderBackend,
                                          public ISceneStillGpuBackend {
 public:
  static constexpr PreviewRenderTargetOwner k_render_target_owner =
      PreviewRenderTargetOwner::MeshPreview;

  MeshPreviewOffscreenBackend();
  ~MeshPreviewOffscreenBackend() override;

  bool initialize(rhi::IRenderBackend* render_backend,
                  AssetManager* asset_manager = nullptr);
  void shutdown();

  bool renderMeshPreview(const MeshAsset& mesh,
                         const MeshPreviewRenderRequest& request,
                         const MeshPreviewCameraFrame& framing,
                         const MeshPreviewStudioLights& lights,
                         MeshPreviewPoseMode pose_mode,
                         eastl::vector<uint8_t>& out_rgba) override;

  /// Multi-draw still with an explicit camera frame (Scene Thumbnail Render).
  bool renderSubmeshDraws(const eastl::vector<MeshPreviewSubmeshDraw>& draws,
                          const MeshPreviewCameraFrame& framing,
                          const MeshPreviewStudioLights& lights, uint32_t width,
                          uint32_t height, eastl::vector<uint8_t>& out_rgba,
                          const SceneInstance* lighting_scene = nullptr) override;

  const rhi::IOffscreenRenderTarget* offscreenTarget() const {
    return m_offscreen.get();
  }

  /// Opaque + transparent draws submitted on the last successful render.
  uint32_t lastSubmittedDrawCount() const { return m_last_submitted_draw_count; }

 private:
  bool ensureResources(uint32_t width, uint32_t height);
  bool ensureRenderPath(uint32_t width, uint32_t height);
  void shutdownRenderPath();

  GpuMesh* getOrUploadGpuMesh(const MeshAsset& mesh_asset,
                              const eastl::string& cache_key);
  VulkanTexture* ensureTextureUploaded(const Texture2DAsset* texture_asset);
  VulkanTexture* getFallbackTexture();

  rhi::IRenderBackend* m_render_backend{nullptr};
  AssetManager* m_asset_manager{nullptr};
  eastl::unique_ptr<rhi::IOffscreenRenderTarget> m_offscreen;
  eastl::unique_ptr<VulkanBuffer> m_readback_staging;
  eastl::unique_ptr<ForwardRenderPath> m_forward_path;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_mesh_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_transparent_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_skinned_mesh_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline>
      m_skinned_transparent_pipeline;
  VulkanTexture* m_fallback_texture{nullptr};
  eastl::unique_ptr<VulkanTexture> m_fallback_texture_owner;
  eastl::unordered_map<eastl::string, eastl::unique_ptr<GpuMesh>> m_gpu_meshes;
  eastl::unordered_map<eastl::string, eastl::unique_ptr<VulkanTexture>>
      m_uploaded_textures;
  uint32_t m_width{0};
  uint32_t m_height{0};
  uint32_t m_pipeline_width{0};
  uint32_t m_pipeline_height{0};
  uint32_t m_last_submitted_draw_count{0};
};

}  // namespace Blunder
