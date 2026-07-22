#pragma once

#include <SDL3/SDL.h>

#include "EASTL/array.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

/// Maps SDL window client coordinates to Slint logical space (HiDPI on Win32).
inline eastl::array<float, 2> mapWindowPointerToLogical(WindowSystem* window_system,
                                                         float x, float y) {
  if (!window_system) {
    return {x, y};
  }

  const eastl::array<int, 2> logical = window_system->getLogicalWindowSize();
  const eastl::array<int, 2> drawable = window_system->getDrawableSize();

#if defined(_WIN32)
  if (logical[0] > 0 && logical[1] > 0) {
    if (drawable[0] > logical[0] + 1 || drawable[1] > logical[1] + 1) {
      return {x * static_cast<float>(logical[0]) / static_cast<float>(drawable[0]),
              y * static_cast<float>(logical[1]) / static_cast<float>(drawable[1])};
    }
    const float display_scale = window_system->getDisplayScale();
    if (display_scale > 1.01f) {
      return {x / display_scale, y / display_scale};
    }
  }
  return {x, y};
#else
  const eastl::array<int, 2> sdl_win = window_system->getWindowSize();
  if (sdl_win[0] > 0 && sdl_win[1] > 0 && logical[0] > 0 && logical[1] > 0 &&
      (logical[0] != sdl_win[0] || logical[1] != sdl_win[1])) {
    return {x * static_cast<float>(logical[0]) / static_cast<float>(sdl_win[0]),
            y * static_cast<float>(logical[1]) / static_cast<float>(sdl_win[1])};
  }
  return {x, y};
#endif
}

/// Maps SDL client coordinates on any window to Slint logical space (HiDPI).
inline eastl::array<float, 2> mapSdlWindowPointerToLogical(SDL_Window* window, float x,
                                                            float y) {
  if (!window) {
    return {x, y};
  }

  int logical_w = 0;
  int logical_h = 0;
  SDL_GetWindowSize(window, &logical_w, &logical_h);
  int drawable_w = 0;
  int drawable_h = 0;
  SDL_GetWindowSizeInPixels(window, &drawable_w, &drawable_h);

#if defined(_WIN32)
  if (logical_w > 0 && logical_h > 0) {
    if (drawable_w > logical_w + 1 || drawable_h > logical_h + 1) {
      return {x * static_cast<float>(logical_w) / static_cast<float>(drawable_w),
              y * static_cast<float>(logical_h) / static_cast<float>(drawable_h)};
    }
    const float display_scale = SDL_GetWindowDisplayScale(window);
    if (display_scale > 1.01f) {
      return {x / display_scale, y / display_scale};
    }
  }
  return {x, y};
#else
  if (logical_w > 0 && logical_h > 0 && drawable_w > 0 && drawable_h > 0 &&
      (logical_w != drawable_w || logical_h != drawable_h)) {
    return {x * static_cast<float>(logical_w) / static_cast<float>(drawable_w),
            y * static_cast<float>(logical_h) / static_cast<float>(drawable_h)};
  }
  return {x, y};
#endif
}

}  // namespace Blunder
