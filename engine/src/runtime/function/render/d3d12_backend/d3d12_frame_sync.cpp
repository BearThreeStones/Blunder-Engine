#include "runtime/function/render/d3d12_backend/d3d12_frame_sync.h"

#include "runtime/function/render/d3d12_backend/d3d12_render_backend.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder::d3d12_backend {

void D3D12FrameSync::bind(D3D12RenderBackend* backend) { m_backend = backend; }

uint32_t D3D12FrameSync::maxFramesInFlight() const {
  return VulkanSync::k_max_frames_in_flight;
}

void D3D12FrameSync::waitForFrame(uint32_t frame_index) {
  if (m_backend) {
    m_backend->waitForGpu(frame_index);
  }
}

void D3D12FrameSync::resetFrameFence(uint32_t frame_index) {
  if (m_backend) {
    m_backend->resetFrameFence(frame_index);
  }
}

void D3D12FrameSync::signalFrameSubmitted(uint32_t frame_index) {
  if (m_backend) {
    m_backend->signalFrameSubmitted(frame_index);
  }
}

}  // namespace Blunder::d3d12_backend
