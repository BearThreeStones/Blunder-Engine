#pragma once

#include "runtime/function/ui/viewport/viewport_cpu_frame.h"

namespace Blunder {

/// Consumes completed viewport readback frames (implemented by SlintViewportSink).
class IViewportSink {
 public:
  virtual ~IViewportSink() = default;

  virtual void presentViewportCpuFrame(const ViewportCpuFrame& frame) = 0;
  virtual void invalidateViewportPlaceholder() = 0;
};

}  // namespace Blunder
