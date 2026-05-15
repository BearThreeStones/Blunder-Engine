#pragma once

#include "runtime/core/event/event.h"

#include <sstream>

namespace Blunder {

// MouseMovedEvent - fired when mouse cursor moves
class MouseMovedEvent : public Event {
 public:
  MouseMovedEvent(float x, float y, float delta_x = 0.0f,
                  float delta_y = 0.0f)
      : m_mouse_x(x),
        m_mouse_y(y),
        m_mouse_delta_x(delta_x),
        m_mouse_delta_y(delta_y) {}

  float getX() const { return m_mouse_x; }
  float getY() const { return m_mouse_y; }
  float getDeltaX() const { return m_mouse_delta_x; }
  float getDeltaY() const { return m_mouse_delta_y; }

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
  float m_mouse_delta_x;
  float m_mouse_delta_y;
};

// MouseScrolledEvent - fired when mouse wheel is scrolled
class MouseScrolledEvent : public Event {
 public:
  MouseScrolledEvent(float x_offset, float y_offset)
      : m_x_offset(x_offset), m_y_offset(y_offset) {}

  MouseScrolledEvent(float x_offset, float y_offset, float mouse_x,
                     float mouse_y)
      : m_x_offset(x_offset),
        m_y_offset(y_offset),
        m_mouse_x(mouse_x),
        m_mouse_y(mouse_y),
        m_has_mouse_position(true) {}

  float getXOffset() const { return m_x_offset; }
  float getYOffset() const { return m_y_offset; }
  float getMouseX() const { return m_mouse_x; }
  float getMouseY() const { return m_mouse_y; }
  bool hasMousePosition() const { return m_has_mouse_position; }

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "MouseScrolledEvent: " << m_x_offset << ", " << m_y_offset;
    if (m_has_mouse_position) {
      ss << " @ " << m_mouse_x << ", " << m_mouse_y;
    }
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(MouseScrolled)
  EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

 private:
  float m_x_offset;
  float m_y_offset;
  float m_mouse_x{0.0f};
  float m_mouse_y{0.0f};
  bool m_has_mouse_position{false};
};

// MouseButtonEvent - base class for mouse button events
class MouseButtonEvent : public Event {
 public:
  int getMouseButton() const { return m_button; }
  float getX() const { return m_mouse_x; }
  float getY() const { return m_mouse_y; }
  bool hasMousePosition() const { return m_has_mouse_position; }

  EVENT_CLASS_CATEGORY(EventCategoryMouseButton | EventCategoryMouse |
                       EventCategoryInput)

 protected:
  MouseButtonEvent(int button) : m_button(button) {}
  MouseButtonEvent(int button, float mouse_x, float mouse_y)
      : m_button(button),
        m_mouse_x(mouse_x),
        m_mouse_y(mouse_y),
        m_has_mouse_position(true) {}

  int m_button;
  float m_mouse_x{0.0f};
  float m_mouse_y{0.0f};
  bool m_has_mouse_position{false};
};

// MouseButtonPressedEvent - fired when a mouse button is pressed
class MouseButtonPressedEvent : public MouseButtonEvent {
 public:
  MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}
  MouseButtonPressedEvent(int button, float mouse_x, float mouse_y)
      : MouseButtonEvent(button, mouse_x, mouse_y) {}

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "MouseButtonPressedEvent: " << m_button;
    if (m_has_mouse_position) {
      ss << " @ " << m_mouse_x << ", " << m_mouse_y;
    }
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(MouseButtonPressed)
};

// MouseButtonReleasedEvent - fired when a mouse button is released
class MouseButtonReleasedEvent : public MouseButtonEvent {
 public:
  MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}
  MouseButtonReleasedEvent(int button, float mouse_x, float mouse_y)
      : MouseButtonEvent(button, mouse_x, mouse_y) {}

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "MouseButtonReleasedEvent: " << m_button;
    if (m_has_mouse_position) {
      ss << " @ " << m_mouse_x << ", " << m_mouse_y;
    }
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(MouseButtonReleased)
};

}  // namespace Blunder
