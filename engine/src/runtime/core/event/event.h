#pragma once

#include <EASTL/functional.h>
#include <EASTL/string.h>

namespace Blunder {

// Bit macro for event categories (bit flags)
#define BIT(x) (1 << (x))

// Event types supported by the engine
enum class EventType {
  None = 0,
  // Window events
  WindowClose,
  WindowResize,
  WindowFocus,
  WindowLostFocus,
  WindowMoved,
  // Key events
  KeyPressed,
  KeyReleased,
  KeyTyped,
  // Mouse events
  MouseButtonPressed,
  MouseButtonReleased,
  MouseMoved,
  MouseScrolled
};

// Event categories for filtering (bit flags)
enum EventCategory {
  EventCategoryNone = 0,
  EventCategoryApplication = BIT(0),
  EventCategoryInput = BIT(1),
  EventCategoryKeyboard = BIT(2),
  EventCategoryMouse = BIT(3),
  EventCategoryMouseButton = BIT(4)
};

// Macros to reduce boilerplate in event classes
#define EVENT_CLASS_TYPE(type)                                              \
  static EventType getStaticType() { return EventType::type; }              \
  virtual EventType getEventType() const override { return getStaticType(); } \
  virtual const char* getName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) \
  virtual int getCategoryFlags() const override { return category; }

// Base Event class - all events inherit from this
class Event {
 public:
  virtual ~Event() = default;

  bool handled = false;

  virtual EventType getEventType() const = 0;
  virtual const char* getName() const = 0;
  virtual int getCategoryFlags() const = 0;

  virtual eastl::string toString() const { return getName(); }

  bool isInCategory(EventCategory category) const {
    return getCategoryFlags() & category;
  }
};

// EventDispatcher - type-safe event dispatch mechanism
// Usage:
//   EventDispatcher dispatcher(event);
//   dispatcher.dispatch<WindowCloseEvent>([](WindowCloseEvent& e) {
//     // handle event
//     return true; // event handled
//   });
class EventDispatcher {
 public:
  EventDispatcher(Event& event) : m_event(event) {}

  // F should be a callable with signature: bool(T&)
  template <typename T, typename F>
  bool dispatch(const F& func) {
    if (m_event.getEventType() == T::getStaticType()) {
      m_event.handled |= func(static_cast<T&>(m_event));
      return true;
    }
    return false;
  }

 private:
  Event& m_event;
};

// Event callback function type
using EventCallbackFn = eastl::function<void(Event&)>;

}  // namespace Blunder
