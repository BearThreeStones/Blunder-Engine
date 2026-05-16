#include "runtime/function/render/presenter/cpu_rgba8_viewport_presenter.h"

#include "runtime/function/slint/slint_system.h"

namespace Blunder::presenter {

CpuRgba8ViewportPresenter::CpuRgba8ViewportPresenter(SlintSystem* slint_system)
    : m_slint_system(slint_system) {}

void CpuRgba8ViewportPresenter::presentCpuRgba8(const uint8_t* pixels,
                                                  uint32_t width,
                                                  uint32_t height,
                                                  uint32_t stride_bytes) {
  if (!m_slint_system || !pixels || width == 0 || height == 0) {
    return;
  }
  (void)stride_bytes;
  m_slint_system->setViewportImage(pixels, width, height);
}

}  // namespace Blunder::presenter
