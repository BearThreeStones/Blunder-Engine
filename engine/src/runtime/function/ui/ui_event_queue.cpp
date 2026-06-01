#include "runtime/function/ui/ui_event_queue.h"

namespace Blunder {

void UiEventQueue::enqueue(UiEvent event) {
  std::lock_guard lock(m_mutex);
  m_events.push_back(eastl::move(event));
}

bool UiEventQueue::tryDequeue(UiEvent& out) {
  std::lock_guard lock(m_mutex);
  if (m_events.empty()) {
    return false;
  }
  out = eastl::move(m_events.front());
  m_events.pop_front();
  return true;
}

void UiEventQueue::clear() {
  std::lock_guard lock(m_mutex);
  m_events.clear();
}

bool UiEventQueue::empty() const {
  std::lock_guard lock(m_mutex);
  return m_events.empty();
}

}  // namespace Blunder
