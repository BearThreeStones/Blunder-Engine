#pragma once

#include "runtime/core/event/event.h"

#include <sstream>

namespace Blunder {

// MouseMovedEvent - fired when mouse cursor moves
class MouseMovedEvent : public Event {
 public:
  MouseMovedEvent(float x, float y) : m_mouse_x(x), m_mouse_y(y) {}

  float getX() const { return m_mouse_x; }
  float getY() const { return m_mouse_y; }

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "MouseMovedEvent: " << m_mouse_x << ", " << m_mouse_y;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(MouseMoved)
  EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

 private:
  float m_mouse_x;
  float m_mouse_y;
};

// MouseScrolledEvent - fired when mouse wheel is scrolled
class MouseScrolledEvent : public Event {
 public:
  MouseScrolledEvent(float x_offset, float y_offset)
      : m_x_offset(x_offset), m_y_offset(y_offset) {}

  float getXOffset() const { return m_x_offset; }
  float getYOffset() const { return m_y_offset; }

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "MouseScrolledEvent: " << m_x_offset << ", " << m_y_offset;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(MouseScrolled)
  EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

 private:
  float m_x_offset;
  float m_y_offset;
};

// MouseButtonEvent - base class for mouse button events
class MouseButtonEvent : public Event {
 public:
  int getMouseButton() const { return m_button; }

  EVENT_CLASS_CATEGORY(EventCategoryMouseButton | EventCategoryMouse |
                       EventCategoryInput)

 protected:
  MouseButtonEvent(int button) : m_button(button) {}

  int m_button;
};

// MouseButtonPressedEvent - fired when a mouse button is pressed
class MouseButtonPressedEvent : public MouseButtonEvent {
 public:
  MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "MouseButtonPressedEvent: " << m_button;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(MouseButtonPressed)
};

// MouseButtonReleasedEvent - fired when a mouse button is released
class MouseButtonReleasedEvent : public MouseButtonEvent {
 public:
  MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "MouseButtonReleasedEvent: " << m_button;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(MouseButtonReleased)
};

}  // namespace Blunder
