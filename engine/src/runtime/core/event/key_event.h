#pragma once

#include "runtime/core/event/event.h"

#include <sstream>

namespace Blunder {

// KeyEvent - base class for all keyboard events
class KeyEvent : public Event {
 public:
  int getKeyCode() const { return m_key_code; }

  EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

 protected:
  KeyEvent(int keycode) : m_key_code(keycode) {}

  int m_key_code;
};

// KeyPressedEvent - fired when a key is pressed
class KeyPressedEvent : public KeyEvent {
 public:
  KeyPressedEvent(int keycode, bool is_repeat = false)
      : KeyEvent(keycode), m_is_repeat(is_repeat) {}

  bool isRepeat() const { return m_is_repeat; }

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "KeyPressedEvent: " << m_key_code << " (repeat = " << m_is_repeat
       << ")";
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(KeyPressed)

 private:
  bool m_is_repeat;
};

// KeyReleasedEvent - fired when a key is released
class KeyReleasedEvent : public KeyEvent {
 public:
  KeyReleasedEvent(int keycode) : KeyEvent(keycode) {}

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "KeyReleasedEvent: " << m_key_code;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(KeyReleased)
};

// KeyTypedEvent - fired for text input (character codes)
class KeyTypedEvent : public KeyEvent {
 public:
  KeyTypedEvent(int keycode) : KeyEvent(keycode) {}

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "KeyTypedEvent: " << m_key_code;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(KeyTyped)
};

}  // namespace Blunder
