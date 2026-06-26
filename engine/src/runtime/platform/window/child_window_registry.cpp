#include "runtime/platform/window/child_window_registry.h"

#include <SDL3/SDL_video.h>

#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

#include "runtime/core/base/macro.h"
#include "runtime/function/slint/window_pointer_map.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

ChildWindowRegistry::ChildWindowRegistry(WindowSystem* parent) : m_parent(parent) {}

void ChildWindowRegistry::reset(WindowSystem* parent) {
  destroyAll();
  m_parent = parent;
}

SDL_Window* ChildWindowRegistry::create(const char* title, int screen_x, int screen_y,
                                        int logical_width, int logical_height) {
  ASSERT(m_parent);
  const int width = eastl::max(logical_width, 160);
  const int height = eastl::max(logical_height, 120);
  SDL_WindowFlags flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  SDL_Window* window =
      SDL_CreateWindow(title != nullptr ? title : "Panel", width, height, flags);
  if (!window) {
    LOG_ERROR("[ChildWindowRegistry] SDL_CreateWindow failed: {}", SDL_GetError());
    return nullptr;
  }
  parentToMain(window);
  SDL_SetWindowPosition(window, screen_x, screen_y);
  SDL_ShowWindow(window);
  m_children.push_back(window);
  return window;
}

void ChildWindowRegistry::destroy(SDL_Window* window) {
  if (!window) {
    return;
  }
  const auto it = eastl::find(m_children.begin(), m_children.end(), window);
  if (it != m_children.end()) {
    m_children.erase(it);
  }
  SDL_DestroyWindow(window);
}

void ChildWindowRegistry::move(SDL_Window* window, int screen_x, int screen_y) {
  if (!window) {
    return;
  }
  SDL_SetWindowPosition(window, screen_x, screen_y);
}

void ChildWindowRegistry::resize(SDL_Window* window, int logical_width, int logical_height) {
  if (!window) {
    return;
  }
  const int width = eastl::max(logical_width, 160);
  const int height = eastl::max(logical_height, 120);
  SDL_SetWindowSize(window, width, height);
}

void ChildWindowRegistry::raise(SDL_Window* window) {
  if (!window) {
    return;
  }
#if defined(_WIN32)
  void* hwnd_ptr = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                          SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
  if (hwnd_ptr != nullptr) {
    SetWindowPos(static_cast<HWND>(hwnd_ptr), HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
#endif
  SDL_RaiseWindow(window);
}

SDL_WindowID ChildWindowRegistry::windowId(SDL_Window* window) const {
  return window ? SDL_GetWindowID(window) : 0;
}

bool ChildWindowRegistry::owns(SDL_Window* window) const {
  return eastl::find(m_children.begin(), m_children.end(), window) != m_children.end();
}

glm::ivec2 ChildWindowRegistry::clientToScreen(float client_x, float client_y) const {
#if defined(_WIN32)
  void* hwnd = m_parent ? m_parent->getNativeWin32Hwnd() : nullptr;
  if (hwnd != nullptr) {
    POINT point{};
    point.x = static_cast<LONG>(client_x + 0.5f);
    point.y = static_cast<LONG>(client_y + 0.5f);
    if (ClientToScreen(static_cast<HWND>(hwnd), &point)) {
      return glm::ivec2{point.x, point.y};
    }
  }
#endif
  if (m_parent && m_parent->getNativeWindow()) {
    int win_x = 0;
    int win_y = 0;
    SDL_GetWindowPosition(m_parent->getNativeWindow(), &win_x, &win_y);
    return glm::ivec2{win_x + static_cast<int>(client_x), win_y + static_cast<int>(client_y)};
  }
  return glm::ivec2{static_cast<int>(client_x), static_cast<int>(client_y)};
}

glm::vec2 ChildWindowRegistry::screenToClient(int screen_x, int screen_y) const {
#if defined(_WIN32)
  void* hwnd = m_parent ? m_parent->getNativeWin32Hwnd() : nullptr;
  if (hwnd != nullptr) {
    POINT point{screen_x, screen_y};
    if (ScreenToClient(static_cast<HWND>(hwnd), &point)) {
      return glm::vec2{static_cast<float>(point.x), static_cast<float>(point.y)};
    }
  }
#endif
  if (m_parent && m_parent->getNativeWindow()) {
    int win_x = 0;
    int win_y = 0;
    SDL_GetWindowPosition(m_parent->getNativeWindow(), &win_x, &win_y);
    return glm::vec2{static_cast<float>(screen_x - win_x),
                     static_cast<float>(screen_y - win_y)};
  }
  return glm::vec2{static_cast<float>(screen_x), static_cast<float>(screen_y)};
}

glm::vec2 ChildWindowRegistry::screenToDockLocal(int screen_x, int screen_y,
                                                  float docking_origin_x,
                                                  float docking_origin_y) const {
  const glm::vec2 client = screenToClient(screen_x, screen_y);
  if (m_parent) {
    const eastl::array<float, 2> logical =
        mapWindowPointerToLogical(m_parent, client.x, client.y);
    return glm::vec2{logical[0] - docking_origin_x, logical[1] - docking_origin_y};
  }
  return glm::vec2{client.x - docking_origin_x, client.y - docking_origin_y};
}

glm::vec2 ChildWindowRegistry::pointerToDockLocal(int screen_x, int screen_y,
                                                  float docking_origin_x,
                                                  float docking_origin_y) const {
  for (SDL_Window* child : m_children) {
    if (!child) {
      continue;
    }
    if ((SDL_GetWindowFlags(child) & SDL_WINDOW_HIDDEN) != 0) {
      continue;
    }
    int win_x = 0;
    int win_y = 0;
    SDL_GetWindowPosition(child, &win_x, &win_y);
    int pixel_w = 0;
    int pixel_h = 0;
    SDL_GetWindowSizeInPixels(child, &pixel_w, &pixel_h);
    if (screen_x < win_x || screen_y < win_y || screen_x >= win_x + pixel_w ||
        screen_y >= win_y + pixel_h) {
      continue;
    }
    const float scale = SDL_GetWindowDisplayScale(child);
    const float pixel_scale = scale > 0.0f ? scale : 1.0f;
    const float local_x = static_cast<float>(screen_x - win_x) / pixel_scale;
    const float local_y = static_cast<float>(screen_y - win_y) / pixel_scale;
    return floatLocalToDockLocal(child, local_x, local_y, docking_origin_x, docking_origin_y);
  }
  return screenToDockLocal(screen_x, screen_y, docking_origin_x, docking_origin_y);
}

glm::vec2 ChildWindowRegistry::floatLocalToDockLocal(
    SDL_Window* float_window, float local_x, float local_y, float docking_origin_x,
    float docking_origin_y) const {
  if (!float_window) {
    return glm::vec2{local_x - docking_origin_x, local_y - docking_origin_y};
  }
  const float scale = SDL_GetWindowDisplayScale(float_window);
  const float pixel_scale = scale > 0.0f ? scale : 1.0f;
  int win_x = 0;
  int win_y = 0;
  SDL_GetWindowPosition(float_window, &win_x, &win_y);
  const int screen_x = win_x + static_cast<int>(local_x * pixel_scale + 0.5f);
  const int screen_y = win_y + static_cast<int>(local_y * pixel_scale + 0.5f);
  const glm::vec2 client = screenToClient(screen_x, screen_y);
  return glm::vec2{client.x - docking_origin_x, client.y - docking_origin_y};
}

void ChildWindowRegistry::destroyAll() {
  while (!m_children.empty()) {
    SDL_Window* window = m_children.back();
    m_children.pop_back();
    SDL_DestroyWindow(window);
  }
}

void ChildWindowRegistry::parentToMain(SDL_Window* child) const {
#if defined(_WIN32)
  if (!m_parent || !child) {
    return;
  }
  void* parent_hwnd = m_parent->getNativeWin32Hwnd();
  void* child_hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(child),
                                            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
  if (parent_hwnd != nullptr && child_hwnd != nullptr) {
    SetWindowLongPtr(static_cast<HWND>(child_hwnd), GWLP_HWNDPARENT,
                     reinterpret_cast<LONG_PTR>(parent_hwnd));
  }
#else
  (void)child;
#endif
}

}  // namespace Blunder
