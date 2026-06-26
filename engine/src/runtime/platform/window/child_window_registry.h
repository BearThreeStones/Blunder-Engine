#pragma once

#include <SDL3/SDL.h>

#include <glm/vec2.hpp>

#include "EASTL/vector.h"

namespace Blunder {

class WindowSystem;

/// Owns borderless SDL child windows parented to the editor main HWND (Win32).
class ChildWindowRegistry final {
 public:
  ChildWindowRegistry() = default;
  explicit ChildWindowRegistry(WindowSystem* parent);

  void reset(WindowSystem* parent);

  SDL_Window* create(const char* title, int screen_x, int screen_y, int logical_width,
                     int logical_height);
  void destroy(SDL_Window* window);
  void move(SDL_Window* window, int screen_x, int screen_y);
  void resize(SDL_Window* window, int logical_width, int logical_height);
  void raise(SDL_Window* window);

  SDL_WindowID windowId(SDL_Window* window) const;
  bool owns(SDL_Window* window) const;

  /// Main-window client coordinates → screen pixels.
  glm::ivec2 clientToScreen(float client_x, float client_y) const;
  /// Screen pixels → main-window client coordinates.
  glm::vec2 screenToClient(int screen_x, int screen_y) const;

  /// Screen pixels → main docking-host local coordinates.
  glm::vec2 screenToDockLocal(int screen_x, int screen_y, float docking_origin_x,
                              float docking_origin_y) const;

  /// Global screen position → dock-local, using child-window path when over a float HWND.
  glm::vec2 pointerToDockLocal(int screen_x, int screen_y, float docking_origin_x,
                               float docking_origin_y) const;

  /// Float-window local logical coords → main docking-host local coords.
  glm::vec2 floatLocalToDockLocal(SDL_Window* float_window, float local_x, float local_y,
                                  float docking_origin_x, float docking_origin_y) const;

  void destroyAll();

 private:
  void parentToMain(SDL_Window* child) const;

  WindowSystem* m_parent{nullptr};
  eastl::vector<SDL_Window*> m_children;
};

}  // namespace Blunder
