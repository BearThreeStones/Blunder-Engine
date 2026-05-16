#pragma once

#include <cstdint>

namespace Blunder::rhi {

class IFrameSync {
 public:
  virtual ~IFrameSync() = default;

  virtual uint32_t maxFramesInFlight() const = 0;
  virtual void waitForFrame(uint32_t frame_index) = 0;
  virtual void resetFrameFence(uint32_t frame_index) = 0;
  virtual void signalFrameSubmitted(uint32_t frame_index) = 0;
};

}  // namespace Blunder::rhi
