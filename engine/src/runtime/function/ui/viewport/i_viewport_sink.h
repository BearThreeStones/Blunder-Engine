#pragma once

#include "runtime/function/ui/viewport/viewport_cpu_frame.h"
#include "runtime/function/ui/viewport/viewport_vulkan_image.h"

namespace Blunder {

/// Consumes completed viewport frames (implemented by SlintViewportSink).
class IViewportSink {
 public:
  virtual ~IViewportSink() = default;

  /// CPU readback path (self-owned UI device / validation enabled).
  virtual void presentViewportCpuFrame(const ViewportCpuFrame& frame) = 0;

  /// Zero-copy path: present a borrowed engine VkImage directly (shared-device).
  virtual void presentViewportVulkanImage(const ViewportVulkanImage& image) = 0;

  virtual void invalidateViewportPlaceholder() = 0;
};

}  // namespace Blunder
