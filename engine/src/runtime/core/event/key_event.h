#pragma once

#include "runtime/core/event/event.h"

#include <EASTL/string.h>

#include <cstdint>
#include <sstream>

namespace Blunder {

/// Modifier bitmask aligned with SDL3 \c SDL_Keymod so WindowSystem can forward \c event.key.mod verbatim.
enum class KeyModifier : uint16_t {
  None = 0x0000,
  LShift = 0x0001,
  RShift = 0x0002,
  Level5 = 0x0004,
  LCtrl = 0x0040,
  RCtrl = 0x0080,
  LAlt = 0x0100,
  RAlt = 0x0200,
  LGui = 0x0400,
  RGui = 0x0800,
  NumLock = 0x1000,
  CapsLock = 0x2000,
  Mode = 0x4000,
  ScrollLock = 0x8000,
};

inline uint16_t keyModifierBits(KeyModifier m) {
  return static_cast<uint16_t>(m);
}

inline bool keyModifiersContain(uint16_t flags, KeyModifier m) {
  return (flags & keyModifierBits(m)) != 0;
}

inline bool keyModifiersCtrl(uint16_t flags) {
  return keyModifiersContain(flags, KeyModifier::LCtrl) ||
         keyModifiersContain(flags, KeyModifier::RCtrl);
}

inline bool keyModifiersShift(uint16_t flags) {
  return keyModifiersContain(flags, KeyModifier::LShift) ||
         keyModifiersContain(flags, KeyModifier::RShift);
}

inline bool keyModifiersAlt(uint16_t flags) {
  return keyModifiersContain(flags, KeyModifier::LAlt) ||
         keyModifiersContain(flags, KeyModifier::RAlt);
}

inline bool keyModifiersGui(uint16_t flags) {
  return keyModifiersContain(flags, KeyModifier::LGui) ||
         keyModifiersContain(flags, KeyModifier::RGui);
}

// KeyEvent - physical / logical key press and release (not IME text composition)
class KeyEvent : public Event {
 public:
  int getKeyCode() const { return m_key_code; }
  int32_t getScanCode() const { return m_scan_code; }
  uint16_t getModifiers() const { return m_modifier_flags; }

  bool isModifierDown(KeyModifier m) const {
    return keyModifiersContain(m_modifier_flags, m);
  }
  bool isCtrlDown() const { return keyModifiersCtrl(m_modifier_flags); }
  bool isShiftDown() const { return keyModifiersShift(m_modifier_flags); }
  bool isAltDown() const { return keyModifiersAlt(m_modifier_flags); }
  bool isGuiDown() const { return keyModifiersGui(m_modifier_flags); }

  EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

 protected:
  KeyEvent(int key_code, int32_t scan_code, uint16_t modifier_flags)
      : m_key_code(key_code),
        m_scan_code(scan_code),
        m_modifier_flags(modifier_flags) {}

  int m_key_code;
  int32_t m_scan_code;
  uint16_t m_modifier_flags;
};

class KeyPressedEvent : public KeyEvent {
 public:
  KeyPressedEvent(int key_code, int32_t scan_code, uint16_t modifier_flags,
                  bool is_repeat = false)
      : KeyEvent(key_code, scan_code, modifier_flags),
        m_is_repeat(is_repeat) {}

  bool isRepeat() const { return m_is_repeat; }

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "KeyPressedEvent: key=" << m_key_code << " scancode=" << m_scan_code
       << " mod=0x" << std::hex << m_modifier_flags << std::dec
       << " repeat=" << m_is_repeat;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(KeyPressed)

 private:
  bool m_is_repeat;
};

class KeyReleasedEvent : public KeyEvent {
 public:
  KeyReleasedEvent(int key_code, int32_t scan_code, uint16_t modifier_flags)
      : KeyEvent(key_code, scan_code, modifier_flags) {}

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "KeyReleasedEvent: key=" << m_key_code << " scancode=" << m_scan_code
       << " mod=0x" << std::hex << m_modifier_flags;
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(KeyReleased)
};

/// UTF-8 text from SDL text input / IME commit (\c SDL_EVENT_TEXT_INPUT).
class KeyTypedEvent : public Event {
 public:
  explicit KeyTypedEvent(eastl::string utf8_text)
      : m_utf8(eastl::move(utf8_text)) {}

  const eastl::string& getUtf8() const { return m_utf8; }

  eastl::string toString() const override {
    std::ostringstream ss;
    ss << "KeyTypedEvent: \"" << m_utf8.c_str() << "\"";
    return eastl::string(ss.str().c_str());
  }

  EVENT_CLASS_TYPE(KeyTyped)
  EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

 private:
  eastl::string m_utf8;
};

}  // namespace Blunder
