#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Blunder {

inline constexpr size_t k_frame_timing_ring_capacity = 120;
inline constexpr size_t k_frame_timing_max_passes = 12;
inline constexpr size_t k_frame_timing_max_cpu_zones = 8;
inline constexpr size_t k_frame_timing_name_bytes = 48;

struct FrameTimingPassGpu {
  char name[k_frame_timing_name_bytes]{};
  float gpu_ms{0.0f};
};

struct FrameTimingCpuZone {
  char name[32]{};
  float cpu_ms{0.0f};
};

struct FrameTimingSlot {
  float cpu_ms{0.0f};
  float gpu_ms{0.0f};
  float fps{0.0f};
  uint32_t instance_count{0};
  uint32_t light_count{0};
  uint32_t batch_count{0};
  /// Surviving meshlet indirect commands (early + late compact counts).
  uint32_t draw_count{0};
  uint32_t pass_count{0};
  uint32_t cpu_zone_count{0};
  FrameTimingPassGpu passes[k_frame_timing_max_passes]{};
  FrameTimingCpuZone cpu_zones[k_frame_timing_max_cpu_zones]{};
};

inline void setFrameTimingName(char* dest, size_t dest_bytes, const char* name) {
  if (dest == nullptr || dest_bytes == 0) {
    return;
  }
  if (name == nullptr) {
    dest[0] = '\0';
    return;
  }
  std::strncpy(dest, name, dest_bytes - 1);
  dest[dest_bytes - 1] = '\0';
}

class FrameTimingRing final {
 public:
  void push(const FrameTimingSlot& slot);
  const FrameTimingSlot& latest() const;
  size_t size() const { return m_count; }
  size_t capacity() const { return k_frame_timing_ring_capacity; }
  /// Oldest-first chronological copy. `out_count` is clamped to `max_count`.
  void copyChronological(FrameTimingSlot* out, size_t max_count,
                         size_t* out_count) const;

 private:
  FrameTimingSlot m_slots[k_frame_timing_ring_capacity]{};
  FrameTimingSlot m_empty{};
  size_t m_next{0};
  size_t m_count{0};
};

}  // namespace Blunder
