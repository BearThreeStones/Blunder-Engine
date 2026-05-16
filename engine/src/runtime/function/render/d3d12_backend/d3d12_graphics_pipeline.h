#pragma once

#include "runtime/function/render/rhi/i_graphics_pipeline.h"

namespace Blunder::d3d12_backend {

/// P0 stub: pipeline creation is not implemented yet.
class D3D12GraphicsPipeline final : public rhi::IGraphicsPipeline {
 public:
  void initialize(rhi::IOffscreenRenderTarget& render_target,
                  const rhi::GraphicsPipelineDesc& desc) override;
  void shutdown() override;

  rhi::ICommandList* commandList(uint32_t frame_index) override;
  const rhi::ICommandList* commandList(uint32_t frame_index) const override;

 private:
  rhi::IOffscreenRenderTarget* m_render_target{nullptr};
};

}  // namespace Blunder::d3d12_backend
