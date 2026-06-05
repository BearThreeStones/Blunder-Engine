#include "runtime/function/ui/viewport/slint_viewport_sink.h"

#include "runtime/function/slint/slint_system.h"

namespace Blunder {

SlintViewportSink::SlintViewportSink(SlintSystem* slint_system)
    : m_slint_system(slint_system) {}

void SlintViewportSink::presentViewportCpuFrame(const ViewportCpuFrame& frame) {
  if (!m_slint_system || frame.pixels_rgba == nullptr || frame.width == 0 ||
      frame.height == 0) {
    return;
  }
  (void)frame.stride_bytes;

  m_slint_system->setViewportImage(frame.pixels_rgba, frame.width, frame.height);
}

void SlintViewportSink::presentViewportVulkanImage(
    const ViewportVulkanImage& image) {
  if (!m_slint_system || image.image == 0 || image.width == 0 ||
      image.height == 0) {
    return;
  }
  m_slint_system->setViewportExternalTexture(image.image, image.format,
                                             image.layout, image.width,
                                             image.height);
}

void SlintViewportSink::invalidateViewportPlaceholder() {
  if (m_slint_system) {
    m_slint_system->applyPendingViewportInvalidate();
  }
}

}  // namespace Blunder
