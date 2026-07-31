#pragma once

#include <cstdint>

#include "EASTL/unique_ptr.h"

#include "runtime/function/render/mesh_preview/mesh_preview_render.h"
#include "runtime/function/render/preview_render_target_owner.h"

namespace Blunder {

class VulkanBuffer;

namespace rhi {
class IOffscreenRenderTarget;
class IRenderBackend;
}  // namespace rhi

/// Owns the dedicated Mesh Preview RT and synchronous CPU readback staging.
/// This owner is intentionally separate from RenderSystem's Camera Preview and
/// main viewport targets. Task 1.3 adds mesh/material draw calls between clear
/// and readback; task 1.2 returns the clear frame.
class MeshPreviewOffscreenBackend final : public IMeshPreviewRenderBackend {
 public:
  static constexpr PreviewRenderTargetOwner k_render_target_owner =
      PreviewRenderTargetOwner::MeshPreview;

  MeshPreviewOffscreenBackend() = default;
  ~MeshPreviewOffscreenBackend() override;

  bool initialize(rhi::IRenderBackend* render_backend);
  void shutdown();

  bool renderMeshPreview(const MeshAsset& mesh,
                         const MeshPreviewRenderRequest& request,
                         const MeshPreviewCameraFrame& framing,
                         const MeshPreviewStudioLights& lights,
                         MeshPreviewPoseMode pose_mode,
                         eastl::vector<uint8_t>& out_rgba) override;

  const rhi::IOffscreenRenderTarget* offscreenTarget() const {
    return m_offscreen.get();
  }

 private:
  bool ensureResources(uint32_t width, uint32_t height);

  rhi::IRenderBackend* m_render_backend{nullptr};
  eastl::unique_ptr<rhi::IOffscreenRenderTarget> m_offscreen;
  eastl::unique_ptr<VulkanBuffer> m_readback_staging;
  uint32_t m_width{0};
  uint32_t m_height{0};
};

}  // namespace Blunder
