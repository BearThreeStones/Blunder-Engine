#pragma once

#include "runtime/core/event/event.h"

#include <sstream>

namespace Blunder {

// WindowResizeEvent - fired when window is resized
class WindowResizeEvent : public Event {
 public:
  WindowResizeEvent(unsigned int width, unsigned int height)
      : m_width(width), m_height(height) {}

  unsigned int getWidth() const { return m_width; }
  unsigned int getHeight() const { return m_height; }

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "WindowResizeEvent: " << m_width << ", " << m_height;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(WindowResize)
  EVENT_CLASS_CATEGORY(EventCategoryApplication)

 private:
  unsigned int m_width;
  unsigned int m_height;
};

// WindowCloseEvent - fired when window close is requested
class WindowCloseEvent : public Event {
 public:
  WindowCloseEvent() = default;

  EVENT_CLASS_TYPE(WindowClose)
  EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

// WindowFocusEvent - fired when window gains focus
class WindowFocusEvent : public Event {
 public:
  WindowFocusEvent() = default;

  EVENT_CLASS_TYPE(WindowFocus)
  EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

// WindowLostFocusEvent - fired when window loses focus
class WindowLostFocusEvent : public Event {
 public:
  WindowLostFocusEvent() = default;

  EVENT_CLASS_TYPE(WindowLostFocus)
  EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

// WindowMovedEvent - fired when window is moved
class WindowMovedEvent : public Event {
 public:
  WindowMovedEvent(int x, int y) : m_x(x), m_y(y) {}

  int getX() const { return m_x; }
  int getY() const { return m_y; }

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "WindowMovedEvent: " << m_x << ", " << m_y;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(WindowMoved)
  EVENT_CLASS_CATEGORY(EventCategoryApplication)

 private:
  int m_x;
  int m_y;
};

}  // namespace Blunder
