#pragma once

#include "runtime/function/render/rhi/i_frame_sync.h"

namespace Blunder {

class VulkanSync;

namespace vulkan_backend {

class VulkanFrameSync final : public rhi::IFrameSync {
 public:
  void bind(VulkanSync* sync);

  uint32_t maxFramesInFlight() const override;
  void waitForFrame(uint32_t frame_index) override;
  void resetFrameFence(uint32_t frame_index) override;
  void signalFrameSubmitted(uint32_t frame_index) override;

  VulkanSync* nativeSync() const { return m_sync; }

 private:
  VulkanSync* m_sync{nullptr};
};

}  // namespace vulkan_backend
}  // namespace Blunder
