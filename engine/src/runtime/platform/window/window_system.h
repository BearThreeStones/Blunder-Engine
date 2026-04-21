#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "EASTL/array.h"
#include "EASTL/functional.h"
#include "EASTL/vector.h"

#include "runtime/core/event/application_event.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"

namespace Blunder {
struct WindowCreateInfo {
  int width{1280};
  int height{720};
  const char* title{"Blunder"};
  bool is_fullscreen{false};
};

class WindowSystem {
 public:
  WindowSystem() = default;
  ~WindowSystem();
  void initialize(WindowCreateInfo create_info);
  void pollEvents() const;
  bool shouldClose() const;
  void setTitle(const char* title);
  GLFWwindow* getWindow() const;
  eastl::array<int, 2> getWindowSize() const;

  typedef eastl::function<void()> onResetFunc;
  typedef eastl::function<void(int, int, int, int)> onKeyFunc;
  typedef eastl::function<void(unsigned int)> onCharFunc;
  typedef eastl::function<void(int, unsigned int)> onCharModsFunc;
  typedef eastl::function<void(int, int, int)> onMouseButtonFunc;
  typedef eastl::function<void(double, double)> onCursorPosFunc;
  typedef eastl::function<void(int)> onCursorEnterFunc;
  typedef eastl::function<void(double, double)> onScrollFunc;
  typedef eastl::function<void(int, const char**)> onDropFunc;
  typedef eastl::function<void(int, int)> onWindowSizeFunc;
  typedef eastl::function<void()> onWindowCloseFunc;

  void registerOnResetFunc(onResetFunc func) { m_onResetFunc.push_back(func); }
  void registerOnKeyFunc(onKeyFunc func) { m_onKeyFunc.push_back(func); }
  void registerOnCharFunc(onCharFunc func) { m_onCharFunc.push_back(func); }
  void registerOnCharModsFunc(onCharModsFunc func) {
    m_onCharModsFunc.push_back(func);
  }
  void registerOnMouseButtonFunc(onMouseButtonFunc func) {
    m_onMouseButtonFunc.push_back(func);
  }
  void registerOnCursorPosFunc(onCursorPosFunc func) {
    m_onCursorPosFunc.push_back(func);
  }
  void registerOnCursorEnterFunc(onCursorEnterFunc func) {
    m_onCursorEnterFunc.push_back(func);
  }
  void registerOnScrollFunc(onScrollFunc func) {
    m_onScrollFunc.push_back(func);
  }
  void registerOnDropFunc(onDropFunc func) { m_onDropFunc.push_back(func); }
  void registerOnWindowSizeFunc(onWindowSizeFunc func) {
    m_onWindowSizeFunc.push_back(func);
  }
  void registerOnWindowCloseFunc(onWindowCloseFunc func) {
    m_onWindowCloseFunc.push_back(func);
  }

  // 事件系统回调
  void setEventCallback(const EventCallbackFn& callback) {
    m_event_callback = callback;
  }

  bool isMouseButtonDown(int button) const {
    if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST) {
      return false;
    }
    return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
  }
  bool getFocusMode() const { return m_is_focus_mode; }
  void setFocusMode(bool mode);

 protected:
  // 窗口事件回调函数
  static void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                          int mods) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->onKey(key, scancode, action, mods);

      // 当事件系统回调触发
      if (app->m_event_callback) {
        switch (action) {
          case GLFW_PRESS: {
            KeyPressedEvent event(key, false);
            app->m_event_callback(event);
            break;
          }
          case GLFW_RELEASE: {
            KeyReleasedEvent event(key);
            app->m_event_callback(event);
            break;
          }
          case GLFW_REPEAT: {
            KeyPressedEvent event(key, true);
            app->m_event_callback(event);
            break;
          }
        }
      }
    }
  }
  static void charCallback(GLFWwindow* window, unsigned int codepoint) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->onChar(codepoint);

      // 触发文字输入的 KeyTypedEvent
      if (app->m_event_callback) {
        KeyTypedEvent event(static_cast<int>(codepoint));
        app->m_event_callback(event);
      }
    }
  }
  static void charModsCallback(GLFWwindow* window, unsigned int codepoint,
                               int mods) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->onCharMods(codepoint, mods);
    }
  }
  static void mouseButtonCallback(GLFWwindow* window, int button, int action,
                                  int mods) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->onMouseButton(button, action, mods);

      // 触发鼠标按钮事件
      if (app->m_event_callback) {
        switch (action) {
          case GLFW_PRESS: {
            MouseButtonPressedEvent event(button);
            app->m_event_callback(event);
            break;
          }
          case GLFW_RELEASE: {
            MouseButtonReleasedEvent event(button);
            app->m_event_callback(event);
            break;
          }
        }
      }
    }
  }
  static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->onCursorPos(xpos, ypos);

      // 触发鼠标移动事件
      if (app->m_event_callback) {
        MouseMovedEvent event(static_cast<float>(xpos),
                              static_cast<float>(ypos));
        app->m_event_callback(event);
      }
    }
  }
  static void cursorEnterCallback(GLFWwindow* window, int entered) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->onCursorEnter(entered);
    }
  }
  static void scrollCallback(GLFWwindow* window, double xoffset,
                             double yoffset) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->onScroll(xoffset, yoffset);

      // 触发鼠标滚动事件
      if (app->m_event_callback) {
        MouseScrolledEvent event(static_cast<float>(xoffset),
                                 static_cast<float>(yoffset));
        app->m_event_callback(event);
      }
    }
  }
  static void dropCallback(GLFWwindow* window, int count, const char** paths) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->onDrop(count, paths);
    }
  }
  static void windowSizeCallback(GLFWwindow* window, int width, int height) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app) {
      app->m_width = width;
      app->m_height = height;

      // 触发窗口缩放事件
      if (app->m_event_callback) {
        WindowResizeEvent event(static_cast<unsigned int>(width),
                                static_cast<unsigned int>(height));
        app->m_event_callback(event);
      }
    }
  }
  static void windowCloseCallback(GLFWwindow* window) {
    WindowSystem* app = (WindowSystem*)glfwGetWindowUserPointer(window);
    if (app && app->m_event_callback) {
      WindowCloseEvent event;
      app->m_event_callback(event);
    }
    glfwSetWindowShouldClose(window, true);
  }

  void onReset() {
    for (auto& func : m_onResetFunc) func();
  }
  void onKey(int key, int scancode, int action, int mods) {
    for (auto& func : m_onKeyFunc) func(key, scancode, action, mods);
  }
  void onChar(unsigned int codepoint) {
    for (auto& func : m_onCharFunc) func(codepoint);
  }
  void onCharMods(int codepoint, unsigned int mods) {
    for (auto& func : m_onCharModsFunc) func(codepoint, mods);
  }
  void onMouseButton(int button, int action, int mods) {
    for (auto& func : m_onMouseButtonFunc) func(button, action, mods);
  }
  void onCursorPos(double xpos, double ypos) {
    for (auto& func : m_onCursorPosFunc) func(xpos, ypos);
  }
  void onCursorEnter(int entered) {
    for (auto& func : m_onCursorEnterFunc) func(entered);
  }
  void onScroll(double xoffset, double yoffset) {
    for (auto& func : m_onScrollFunc) func(xoffset, yoffset);
  }
  void onDrop(int count, const char** paths) {
    for (auto& func : m_onDropFunc) func(count, paths);
  }
  void onWindowSize(int width, int height) {
    for (auto& func : m_onWindowSizeFunc) func(width, height);
  }

 private:
  GLFWwindow* m_window{nullptr};
  int m_width{0};
  int m_height{0};

  bool m_is_focus_mode{false};

  eastl::vector<onResetFunc> m_onResetFunc;
  eastl::vector<onKeyFunc> m_onKeyFunc;
  eastl::vector<onCharFunc> m_onCharFunc;
  eastl::vector<onCharModsFunc> m_onCharModsFunc;
  eastl::vector<onMouseButtonFunc> m_onMouseButtonFunc;
  eastl::vector<onCursorPosFunc> m_onCursorPosFunc;
  eastl::vector<onCursorEnterFunc> m_onCursorEnterFunc;
  eastl::vector<onScrollFunc> m_onScrollFunc;
  eastl::vector<onDropFunc> m_onDropFunc;
  eastl::vector<onWindowSizeFunc> m_onWindowSizeFunc;
  eastl::vector<onWindowCloseFunc> m_onWindowCloseFunc;

  // 事件系统回调
  EventCallbackFn m_event_callback;
};
}  // namespace Blunder