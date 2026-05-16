#pragma once

#include "runtime/function/render/presenter/i_viewport_presenter.h"

namespace Blunder {

class SlintSystem;

namespace presenter {

class CpuRgba8ViewportPresenter final : public rhi::IViewportPresenter {
 public:
  explicit CpuRgba8ViewportPresenter(SlintSystem* slint_system);

  void presentCpuRgba8(const uint8_t* pixels, uint32_t width, uint32_t height,
                       uint32_t stride_bytes) override;

 private:
  SlintSystem* m_slint_system{nullptr};
};

}  // namespace presenter
}  // namespace Blunder
