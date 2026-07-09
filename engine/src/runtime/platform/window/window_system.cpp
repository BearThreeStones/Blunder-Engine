#include "runtime/platform/window/window_system.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "runtime/core/base/macro.h"
#include "runtime/core/event/application_event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"

namespace Blunder {
WindowSystem::~WindowSystem() { shutdown(); }

void WindowSystem::initialize(WindowCreateInfo create_info) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    LOG_FATAL("[WindowSystem::initialize] failed to initialize SDL3: {}",
              SDL_GetError());
  }

  m_width = create_info.width;
  m_height = create_info.height;

  // Slint's source-built SkiaRenderer owns the presentation path on this HWND.
  // The editor therefore keeps the SDL window out of SDL_WINDOW_VULKAN mode:
  // the engine's Vulkan context still renders to off-screen images and feeds
  // the UI via CPU readback, while Slint handles composition and present.
  SDL_WindowFlags window_flags =
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (create_info.is_fullscreen) {
    window_flags |= SDL_WINDOW_FULLSCREEN;
  }

  m_window =
      SDL_CreateWindow(create_info.title, create_info.width, create_info.height,
                       window_flags);
  if (!m_window) {
    LOG_FATAL("[WindowSystem::initialize] failed to create SDL window: {}",
              SDL_GetError());
  }

  if (!SDL_GetWindowSize(m_window, &m_width, &m_height)) {
    LOG_FATAL("[WindowSystem::initialize] failed to get SDL window size: {}",
              SDL_GetError());
  }

  if (!SDL_SetWindowMinimumSize(m_window, 960, 600)) {
    LOG_WARN("[WindowSystem::initialize] failed to set minimum window size: {}",
             SDL_GetError());
  }

  m_should_close = false;
  m_is_focus_mode = false;
  SDL_ShowWindow(m_window);
  SDL_StartTextInput(m_window);
}

void WindowSystem::shutdown() {
  m_native_event_callback = {};
  m_event_callback = {};

  clearSystemCursor();
  if (m_window) {
    SDL_StopTextInput(m_window);
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }

  SDL_Quit();
  m_should_close = false;
  m_is_focus_mode = false;
}

void WindowSystem::pumpEvents() {
  m_resize_notify_pending = false;
  // SDL_PollEvent can block for the entire interactive resize (see SDL
  // AppFreezeDuringDrag). Pump Win32 messages once per frame, then drain the
  // SDL queue without blocking.
  if (!m_win32_modal_size_loop) {
    SDL_PumpEvents();
  }
  SDL_Event event;
  while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_FIRST,
                         SDL_EVENT_LAST) > 0) {
    if (m_native_event_callback) {
      m_native_event_callback(event);
    }
    processEvent(event);
  }
  if (m_resize_notify_pending) {
    refreshWindowPixelSize();
    m_resize_notify_pending = false;
  }
}

void WindowSystem::dispatchApplicationEvent(const SDL_Event& event,
                                            bool route_to_input_layers) {
  if (m_native_event_callback) {
    m_native_event_callback(event);
  }
  if (route_to_input_layers) {
    processEvent(event);
  }
  if (m_resize_notify_pending) {
    refreshWindowPixelSize();
    m_resize_notify_pending = false;
  }
}

bool WindowSystem::shouldClose() const { return m_should_close; }

void WindowSystem::requestClose() { m_should_close = true; }

void WindowSystem::setTitle(const char* title) {
  ASSERT(m_window);
  if (!SDL_SetWindowTitle(m_window, title)) {
    LOG_WARN("[WindowSystem::setTitle] failed to set title: {}",
             SDL_GetError());
  }
}

SDL_WindowID WindowSystem::getWindowId() const {
  return m_window ? SDL_GetWindowID(m_window) : 0;
}

eastl::array<int, 2> WindowSystem::getWindowSize() const {
  int width = 0;
  int height = 0;
  if (m_window) {
    SDL_GetWindowSize(m_window, &width, &height);
  }
  return eastl::array<int, 2>({width, height});
}

eastl::array<int, 2> WindowSystem::getLogicalWindowSize() const {
  int width = 0;
  int height = 0;
  if (m_window) {
    SDL_GetWindowSize(m_window, &width, &height);
  }
#if defined(_WIN32)
  if (m_window) {
    void* hwnd = getNativeWin32Hwnd();
    if (hwnd != nullptr) {
      RECT client_rect{};
      if (GetClientRect(static_cast<HWND>(hwnd), &client_rect)) {
        const int client_w =
            eastl::max(static_cast<int>(client_rect.right - client_rect.left), 1);
        const int client_h =
            eastl::max(static_cast<int>(client_rect.bottom - client_rect.top), 1);
        const float display_scale = getDisplayScale();
        const int client_logical_w =
            static_cast<int>(static_cast<float>(client_w) / display_scale + 0.5f);
        const int client_logical_h =
            static_cast<int>(static_cast<float>(client_h) / display_scale + 0.5f);
        // SDL can lag low after maximize; if SDL overshoots client-derived logical
        // (pixels mistaken for points), trust the client rect so scale stays correct.
        if (width > client_logical_w + 2) {
          width = client_logical_w;
        }
        if (height > client_logical_h + 2) {
          height = client_logical_h;
        }
        if (client_logical_w > width) {
          width = client_logical_w;
        }
        if (client_logical_h > height) {
          height = client_logical_h;
        }
      }
    }
  }
#endif
  return eastl::array<int, 2>({eastl::max(width, 1), eastl::max(height, 1)});
}

float WindowSystem::getDisplayScale() const {
  if (!m_window) {
    return 1.0f;
  }
  const float scale = SDL_GetWindowDisplayScale(m_window);
  return scale > 0.0f ? scale : 1.0f;
}

void WindowSystem::notifyClientPixelSize(int width, int height) {
  if (width > 0 && height > 0) {
    m_client_pixel_w = width;
    m_client_pixel_h = height;
  }
}

void WindowSystem::refreshClientPixelSizeFromHwnd() {
#if defined(_WIN32)
  void* hwnd = getNativeWin32Hwnd();
  if (hwnd == nullptr) {
    return;
  }
  RECT client_rect{};
  if (!GetClientRect(static_cast<HWND>(hwnd), &client_rect)) {
    return;
  }
  const int client_w =
      static_cast<int>(client_rect.right - client_rect.left);
  const int client_h =
      static_cast<int>(client_rect.bottom - client_rect.top);
  if (client_w > 0 && client_h > 0) {
    notifyClientPixelSize(client_w, client_h);
  }
#endif
}

eastl::array<int, 2> WindowSystem::getDrawableSize() const {
  int width = 0;
  int height = 0;
  if (m_window) {
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
  }
#if defined(_WIN32)
  if (m_client_pixel_w > 0 && m_client_pixel_h > 0) {
    width = eastl::max(width, m_client_pixel_w);
    height = eastl::max(height, m_client_pixel_h);
  }
  if (m_window) {
    void* hwnd = getNativeWin32Hwnd();
    if (hwnd != nullptr) {
      RECT client_rect{};
      if (GetClientRect(static_cast<HWND>(hwnd), &client_rect)) {
        const int client_w =
            eastl::max(static_cast<int>(client_rect.right - client_rect.left), 1);
        const int client_h =
            eastl::max(static_cast<int>(client_rect.bottom - client_rect.top), 1);
        width = eastl::max(width, client_w);
        height = eastl::max(height, client_h);
      }
    }
  }
#endif
  return eastl::array<int, 2>({eastl::max(width, 1), eastl::max(height, 1)});
}

bool WindowSystem::isMouseButtonDown(int button) const {
  const SDL_MouseButtonFlags buttons = SDL_GetMouseState(nullptr, nullptr);
  return (buttons & SDL_BUTTON_MASK(button)) != 0;
}

void WindowSystem::setFocusMode(bool mode) {
  if (!m_window) {
    return;
  }

  m_is_focus_mode = mode;
  if (!SDL_SetWindowRelativeMouseMode(m_window, mode)) {
    LOG_WARN("[WindowSystem::setFocusMode] failed to set relative mouse mode: {}",
             SDL_GetError());
  }

  if (mode) {
    SDL_HideCursor();
    SDL_SetWindowMouseGrab(m_window, true);
  } else {
    SDL_ShowCursor();
    SDL_SetWindowMouseGrab(m_window, false);
  }
}

void WindowSystem::setSystemCursor(const SDL_SystemCursor cursor) {
  SDL_Cursor* sdl_cursor = SDL_CreateSystemCursor(cursor);
  if (sdl_cursor == nullptr) {
    LOG_WARN("[WindowSystem::setSystemCursor] failed: {}", SDL_GetError());
    return;
  }
  SDL_Cursor* old_cursor = m_system_cursor;
  SDL_SetCursor(sdl_cursor);
  if (old_cursor != nullptr) {
    SDL_DestroyCursor(old_cursor);
  }
  m_system_cursor = sdl_cursor;
}

void WindowSystem::clearSystemCursor() {
  if (m_system_cursor != nullptr) {
    SDL_SetCursor(SDL_GetDefaultCursor());
    SDL_DestroyCursor(m_system_cursor);
    m_system_cursor = nullptr;
  }
}

bool WindowSystem::createVulkanSurface(VkInstance instance,
                                       VkSurfaceKHR* surface) const {
  ASSERT(m_window);
  ASSERT(surface);
  return SDL_Vulkan_CreateSurface(m_window, instance, nullptr, surface);
}

void* WindowSystem::getNativeWin32Hwnd() const {
  if (!m_window) {
    return nullptr;
  }
  SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
  if (!props) {
    return nullptr;
  }
  return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                nullptr);
}

void* WindowSystem::getNativeWin32HInstance() const {
  if (!m_window) {
    return nullptr;
  }
  SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
  if (props) {
    if (void* hinstance = SDL_GetPointerProperty(
            props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr)) {
      return hinstance;
    }
  }
#ifdef _WIN32
  return reinterpret_cast<void*>(GetModuleHandleW(nullptr));
#else
  return nullptr;
#endif
}

eastl::array<const char*, 16> WindowSystem::getRequiredVulkanExtensions(
    uint32_t* count) const {
  ASSERT(count);

  uint32_t extension_count = 0;
  const char* const* extensions =
      SDL_Vulkan_GetInstanceExtensions(&extension_count);
  if (!extensions || extension_count == 0) {
    LOG_FATAL(
        "[WindowSystem::getRequiredVulkanExtensions] failed to query extensions: {}",
        SDL_GetError());
  }

  ASSERT(extension_count <= 16);
  eastl::array<const char*, 16> result{};
  for (uint32_t i = 0; i < extension_count; ++i) {
    result[i] = extensions[i];
  }

  *count = extension_count;
  return result;
}

bool WindowSystem::processEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_QUIT:
      requestClose();
      if (m_event_callback) {
        WindowCloseEvent close_event;
        m_event_callback(close_event);
      }
      return true;
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_MOVED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    case SDL_EVENT_WINDOW_MOUSE_ENTER:
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
      handleWindowEvent(event);
      return true;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      handleKeyboardEvent(event);
      return true;
    case SDL_EVENT_TEXT_INPUT:
      handleTextInputEvent(event);
      return true;
    case SDL_EVENT_MOUSE_MOTION:
      handleMouseMotionEvent(event);
      return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      handleMouseButtonEvent(event);
      return true;
    case SDL_EVENT_MOUSE_WHEEL:
      handleMouseWheelEvent(event);
      return true;
    case SDL_EVENT_DROP_FILE:
    case SDL_EVENT_DROP_TEXT:
      handleDropEvent(event);
      return true;
    default:
      return false;
  }
}

void WindowSystem::refreshWindowPixelSize() {
  if (!m_window) {
    return;
  }

  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSizeInPixels(m_window, &width, &height)) {
    LOG_WARN("[WindowSystem::refreshWindowPixelSize] failed: {}", SDL_GetError());
    return;
  }
  if (width <= 0 || height <= 0) {
    return;
  }

  m_width = width;
  m_height = height;
  if (m_event_callback) {
    WindowResizeEvent resize_event(static_cast<unsigned int>(width),
                                   static_cast<unsigned int>(height));
    m_event_callback(resize_event);
  }
}

void WindowSystem::handleWindowEvent(const SDL_Event& event) {
  if (event.window.windowID != getWindowId()) {
    return;
  }

  switch (event.type) {
    case SDL_EVENT_WINDOW_MOVED: {
      if (m_event_callback) {
        WindowMovedEvent moved_event(event.window.data1, event.window.data2);
        m_event_callback(moved_event);
      }
      break;
    }
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
    case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED: {
      m_resize_notify_pending = true;
      break;
    }
    case SDL_EVENT_WINDOW_FOCUS_GAINED: {
      if (m_event_callback) {
        WindowFocusEvent focus_event;
        m_event_callback(focus_event);
      }
      break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST: {
      if (m_event_callback) {
        WindowLostFocusEvent focus_event;
        m_event_callback(focus_event);
      }
      break;
    }
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
      requestClose();
      if (m_event_callback) {
        WindowCloseEvent close_event;
        m_event_callback(close_event);
      }
      break;
    }
    default:
      break;
  }
}

void WindowSystem::handleKeyboardEvent(const SDL_Event& event) {
  if (event.key.windowID != getWindowId() || !m_event_callback) {
    return;
  }

  if (event.type == SDL_EVENT_KEY_DOWN) {
    KeyPressedEvent key_event(static_cast<int>(event.key.key),
                              static_cast<int32_t>(event.key.scancode),
                              static_cast<uint16_t>(event.key.mod), event.key.repeat);
    m_event_callback(key_event);
  } else {
    KeyReleasedEvent key_event(static_cast<int>(event.key.key),
                               static_cast<int32_t>(event.key.scancode),
                               static_cast<uint16_t>(event.key.mod));
    m_event_callback(key_event);
  }
}

void WindowSystem::handleTextInputEvent(const SDL_Event& event) {
  if (event.text.windowID != getWindowId() || !m_event_callback ||
      !event.text.text) {
    return;
  }

  if (event.text.text[0] == '\0') {
    return;
  }

  KeyTypedEvent key_event(eastl::string(event.text.text));
  m_event_callback(key_event);
}

void WindowSystem::handleMouseMotionEvent(const SDL_Event& event) {
  if (event.motion.windowID != getWindowId() || !m_event_callback) {
    return;
  }

  MouseMovedEvent mouse_event(event.motion.x, event.motion.y,
                              event.motion.xrel, event.motion.yrel);
  m_event_callback(mouse_event);
}

void WindowSystem::handleMouseButtonEvent(const SDL_Event& event) {
  if (event.button.windowID != getWindowId() || !m_event_callback) {
    return;
  }

  const int button = static_cast<int>(event.button.button);
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    MouseButtonPressedEvent button_event(button, event.button.x,
                                         event.button.y);
    m_event_callback(button_event);
  } else {
    MouseButtonReleasedEvent button_event(button, event.button.x,
                                          event.button.y);
    m_event_callback(button_event);
  }
}

void WindowSystem::handleMouseWheelEvent(const SDL_Event& event) {
  if (event.wheel.windowID != getWindowId() || !m_event_callback) {
    return;
  }

  const float x = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                      ? -event.wheel.x
                      : event.wheel.x;
  const float y = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                      ? -event.wheel.y
                      : event.wheel.y;
  MouseScrolledEvent scroll_event(x, y, event.wheel.mouse_x,
                                  event.wheel.mouse_y);
  m_event_callback(scroll_event);
}

void WindowSystem::handleDropEvent(const SDL_Event& event) {
  if (event.drop.windowID != getWindowId() || !event.drop.data) {
    return;
  }

  LOG_INFO("[WindowSystem] drop event: {}", event.drop.data);
}
}  // namespace Blunder
