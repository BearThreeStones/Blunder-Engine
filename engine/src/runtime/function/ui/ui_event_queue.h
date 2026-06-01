#pragma once

#include <mutex>

#include "EASTL/deque.h"

#include "runtime/function/ui/ui_events.h"

namespace Blunder {

/// Thread-safe UI command queue (producers: Slint callbacks; consumer: main thread).
class UiEventQueue final {
 public:
  void enqueue(UiEvent event);
  bool tryDequeue(UiEvent& out);
  void clear();
  bool empty() const;

 private:
  mutable std::mutex m_mutex;
  eastl::deque<UiEvent> m_events;
};

}  // namespace Blunder
