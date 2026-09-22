#include "runtime/function/debug/frame_timing_ring.h"

namespace Blunder {

void FrameTimingRing::push(const FrameTimingSlot& slot) {
  m_slots[m_next] = slot;
  m_next = (m_next + 1) % k_frame_timing_ring_capacity;
  if (m_count < k_frame_timing_ring_capacity) {
    ++m_count;
  }
}

const FrameTimingSlot& FrameTimingRing::latest() const {
  if (m_count == 0) {
    return m_empty;
  }
  const size_t index =
      (m_next + k_frame_timing_ring_capacity - 1) % k_frame_timing_ring_capacity;
  return m_slots[index];
}

void FrameTimingRing::copyChronological(FrameTimingSlot* out, size_t max_count,
                                        size_t* out_count) const {
  const size_t n = m_count < max_count ? m_count : max_count;
  if (out_count != nullptr) {
    *out_count = n;
  }
  if (out == nullptr || n == 0) {
    return;
  }
  size_t start = 0;
  if (m_count == k_frame_timing_ring_capacity) {
    start = m_next;
  }
  for (size_t i = 0; i < n; ++i) {
    out[i] = m_slots[(start + i) % k_frame_timing_ring_capacity];
  }
}

}  // namespace Blunder
