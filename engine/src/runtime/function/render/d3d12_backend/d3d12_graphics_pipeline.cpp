#include "runtime/function/render/d3d12_backend/d3d12_graphics_pipeline.h"

#include "runtime/core/base/macro.h"

namespace Blunder::d3d12_backend {

void D3D12GraphicsPipeline::initialize(rhi::IOffscreenRenderTarget& render_target,
                                       const rhi::GraphicsPipelineDesc& desc) {
  (void)desc;
  m_render_target = &render_target;
  LOG_WARN(
      "[D3D12GraphicsPipeline] P0 stub: graphics pipelines are not implemented");
}

void D3D12GraphicsPipeline::shutdown() { m_render_target = nullptr; }

rhi::ICommandList* D3D12GraphicsPipeline::commandList(uint32_t frame_index) {
  (void)frame_index;
  return nullptr;
}

const rhi::ICommandList* D3D12GraphicsPipeline::commandList(
    uint32_t frame_index) const {
  (void)frame_index;
  return nullptr;
}

}  // namespace Blunder::d3d12_backend
