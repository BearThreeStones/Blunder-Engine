#pragma once

#include <cstdint>

namespace Blunder {

class HighPrecisionTimer final {
 public:
  void reset();
  float tick();

 private:
  static constexpr int64_t k_nanoseconds_per_second{1000000000LL};

  static int64_t queryCurrentTick();
  static int64_t queryFrequency();

 private:
  int64_t m_frequency{0};
  int64_t m_last_tick{0};
  int64_t m_delta_tick{0};
};

}  // namespace Blunder
