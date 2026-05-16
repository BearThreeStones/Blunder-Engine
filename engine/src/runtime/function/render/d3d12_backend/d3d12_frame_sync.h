#pragma once

#include "runtime/function/render/rhi/i_frame_sync.h"

namespace Blunder {

namespace d3d12_backend {

class D3D12RenderBackend;

class D3D12FrameSync final : public rhi::IFrameSync {
 public:
  void bind(D3D12RenderBackend* backend);

  uint32_t maxFramesInFlight() const override;
  void waitForFrame(uint32_t frame_index) override;
  void resetFrameFence(uint32_t frame_index) override;
  void signalFrameSubmitted(uint32_t frame_index) override;

 private:
  D3D12RenderBackend* m_backend{nullptr};
};

}  // namespace d3d12_backend
}  // namespace Blunder
