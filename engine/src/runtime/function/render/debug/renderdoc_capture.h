#pragma once

#include <vulkan/vulkan.h>

namespace Blunder {

/// Thin wrapper around the RenderDoc In-Application API.
///
/// Because the engine renders headlessly (no swapchain / no
/// vkQueuePresentKHR), RenderDoc cannot rely on its usual Present hook to
/// detect frame boundaries. Instead we explicitly mark frames with
/// StartFrameCapture / EndFrameCapture, bound to the engine's VkInstance, so
/// captures only contain the engine's Vulkan work and exclude whatever Slint
/// / Skia happens to do on the window's HWND.
///
/// Usage:
///   * Construct + initialize() once at startup.
///   * triggerCapture() schedules a single-frame capture (e.g. from an F11
///     hotkey). It is a no-op when RenderDoc is not attached.
///   * beginFrame() / endFrame() must bracket the frame's Vulkan submissions.
///
/// initialize() succeeds (silently) even when RenderDoc is not attached;
/// isAttached() can be used to gate UI affordances.
class RenderDocCapture final {
 public:
  RenderDocCapture() = default;
  ~RenderDocCapture() = default;

  RenderDocCapture(const RenderDocCapture&) = delete;
  RenderDocCapture& operator=(const RenderDocCapture&) = delete;

  /// Detects an already-loaded RenderDoc module (renderdoc.dll on Windows,
  /// librenderdoc.so on Linux) and resolves the API entry. Does nothing if
  /// the host process was not launched / injected by RenderDoc.
  void initialize();
  void shutdown();

  bool isAttached() const { return m_api != nullptr; }

  /// Requests that the next call to beginFrame() starts a capture. Calling
  /// this when no capture is pending is harmless. Has no effect when
  /// RenderDoc is not attached.
  void triggerCapture();

  /// Opens a capture for the supplied VkInstance if one is pending. Must be
  /// paired with endFrame() on the same instance later in the frame. Safe to
  /// call every frame; only does work when a capture was triggered.
  void beginFrame(VkInstance instance);

  /// Closes the in-progress capture, if any. Safe to call when no capture is
  /// active.
  void endFrame(VkInstance instance);

 private:
  /// Opaque RENDERDOC_API_1_6_0*. Stored as void* to keep renderdoc_app.h out
  /// of this header so the only translation unit that needs it is the .cpp.
  void* m_api{nullptr};
  bool m_capture_pending{false};
  bool m_capture_active{false};
};

}  // namespace Blunder
