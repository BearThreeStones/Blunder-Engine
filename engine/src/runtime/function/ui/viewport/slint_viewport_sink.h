#pragma once

#include "runtime/function/ui/viewport/i_viewport_sink.h"

namespace Blunder {

class SlintSystem;

class SlintViewportSink final : public IViewportSink {
 public:
  explicit SlintViewportSink(SlintSystem* slint_system);

  void presentViewportCpuFrame(const ViewportCpuFrame& frame) override;
  void presentViewportVulkanImage(const ViewportVulkanImage& image) override;
  void invalidateViewportPlaceholder() override;

 private:
  SlintSystem* m_slint_system{nullptr};
};

}  // namespace Blunder
