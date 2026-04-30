#include "runtime/core/base/high_precision_timer.h"

#include <chrono>
#include <stdexcept>

#ifdef _WIN32
  #include <Windows.h>
#endif

namespace Blunder {

void HighPrecisionTimer::reset() {
  m_frequency = queryFrequency();
  m_last_tick = queryCurrentTick();
  m_delta_tick = 0;
}

float HighPrecisionTimer::tick() {
  if (m_frequency == 0) {
    reset();
  }

  const int64_t current_tick = queryCurrentTick();
  m_delta_tick = current_tick - m_last_tick;
  m_last_tick = current_tick;

  return static_cast<float>(static_cast<double>(m_delta_tick) /
                            static_cast<double>(m_frequency));
}

int64_t HighPrecisionTimer::queryCurrentTick() {
#ifdef _WIN32
  LARGE_INTEGER counter;
  if (QueryPerformanceCounter(&counter) == 0) {
    throw std::runtime_error("Failed to query performance counter");
  }

  return static_cast<int64_t>(counter.QuadPart);
#else
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
      .count();
#endif
}

int64_t HighPrecisionTimer::queryFrequency() {
#ifdef _WIN32
  LARGE_INTEGER frequency;
  if (QueryPerformanceFrequency(&frequency) == 0) {
    throw std::runtime_error("Failed to query performance counter frequency");
  }

  return static_cast<int64_t>(frequency.QuadPart);
#else
  return k_nanoseconds_per_second;
#endif
}

}  // namespace Blunder
