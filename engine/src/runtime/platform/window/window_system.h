#pragma once

#include <SDL3/SDL.h>

#include <vulkan/vulkan.h>

#include "EASTL/array.h"
#include "EASTL/functional.h"

#include "runtime/core/event/event.h"

namespace Blunder {
struct WindowCreateInfo {
  int width{1280};
  int height{720};
  const char* title{"Blunder"};
  bool is_fullscreen{false};
};

class WindowSystem final {
 public:
  WindowSystem() = default;
  ~WindowSystem();

  void initialize(WindowCreateInfo create_info);
  void shutdown();
  void pumpEvents();
  bool shouldClose() const;
  void requestClose();
  void setTitle(const char* title);

  SDL_Window* getNativeWindow() const { return m_window; }
  SDL_WindowID getWindowId() const;
  eastl::array<int, 2> getWindowSize() const;
  eastl::array<int, 2> getDrawableSize() const;

  bool isMouseButtonDown(int button) const;
  bool getFocusMode() const { return m_is_focus_mode; }
  void setFocusMode(bool mode);

  bool createVulkanSurface(VkInstance instance, VkSurfaceKHR* surface) const;
  eastl::array<const char*, 16> getRequiredVulkanExtensions(
      uint32_t* count) const;

  /// Native Win32 HWND backing this window, or nullptr if not on Windows.
  void* getNativeWin32Hwnd() const;
  /// Native Win32 HINSTANCE associated with the process, or nullptr.
  void* getNativeWin32HInstance() const;

  void setEventCallback(const EventCallbackFn& callback) {
    m_event_callback = callback;
  }
  void setNativeEventCallback(const eastl::function<void(const SDL_Event&)>& callback) {
    m_native_event_callback = callback;
  }

  bool processEvent(const SDL_Event& event);

 private:
  void handleWindowEvent(const SDL_Event& event);
  void handleKeyboardEvent(const SDL_Event& event);
  void handleTextInputEvent(const SDL_Event& event);
  void handleMouseMotionEvent(const SDL_Event& event);
  void handleMouseButtonEvent(const SDL_Event& event);
  void handleMouseWheelEvent(const SDL_Event& event);
  void handleDropEvent(const SDL_Event& event);

  SDL_Window* m_window{nullptr};
  int m_width{0};
  int m_height{0};
  bool m_should_close{false};
  bool m_is_focus_mode{false};
  EventCallbackFn m_event_callback;
  eastl::function<void(const SDL_Event&)> m_native_event_callback;
};
}  // namespace Blunder
