#pragma once

#include <cstdint>

namespace Blunder::rhi {

class IViewportPresenter {
 public:
  virtual ~IViewportPresenter() = default;

  virtual void presentCpuRgba8(const uint8_t* pixels, uint32_t width,
                               uint32_t height, uint32_t stride_bytes) = 0;
};

}  // namespace Blunder::rhi
