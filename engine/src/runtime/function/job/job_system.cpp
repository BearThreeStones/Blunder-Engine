#include "runtime/function/job/job_system.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/debug/frame_timing_service.h"
#include "runtime/function/debug/tracy_instrument.h"
#include "runtime/function/global/global_context.h"

#include <cstdio>
#include <cstdlib>

namespace Blunder {

JobSystem::~JobSystem() {
  try {
    shutdown();
  } catch (...) {
    std::abort();
  }
}

uint32_t JobSystem::defaultDedicatedWorkerCount() {
  const unsigned hardware_threads = std::thread::hardware_concurrency();
  if (hardware_threads == 0) {
    return 0;
  }
  return hardware_threads - 1;
}

void JobSystem::initialize() {
  initialize(defaultDedicatedWorkerCount());
}

void JobSystem::joinWorkers() {
  {
    std::lock_guard lock(m_mutex);
    m_stop = true;
  }
  m_wake_cv.notify_all();
  for (std::thread& worker : m_workers) {
    if (worker.joinable()) {
      try {
        worker.join();
      } catch (...) {
        std::abort();
      }
    }
  }
  m_workers.clear();
}

void JobSystem::initialize(uint32_t dedicated_worker_count) {
  shutdown();
  m_owner_thread = std::this_thread::get_id();
  {
    std::lock_guard lock(m_mutex);
    m_stop = false;
    m_outstanding = 0;
    m_queue.clear();
  }
  m_range_jobs.clear();
  m_dedicated_worker_count = dedicated_worker_count;
  m_workers.reserve(dedicated_worker_count);
  try {
    for (uint32_t i = 0; i < dedicated_worker_count; ++i) {
      m_workers.emplace_back([this, i]() {
#ifdef TRACY_ENABLE
        char name[32];
        std::snprintf(name, sizeof(name), "Job %u", i);
        tracy::SetThreadName(name);
#endif
        workerLoop();
      });
    }
  } catch (...) {
    joinWorkers();
    throw;
  }
  m_initialized = true;
}

void JobSystem::shutdown() {
  if (!m_initialized && m_workers.empty()) {
    return;
  }
  if (m_initialized) {
    assertOwner();
    wait();
  }
  joinWorkers();
  m_queue.clear();
  m_range_jobs.clear();
  m_outstanding = 0;
  m_initialized = false;
}

void JobSystem::submit(JobFunction function, void* job_data) {
  ASSERT(m_initialized);
  assertOwner();
  ASSERT(function != nullptr);
  {
    std::lock_guard lock(m_mutex);
    ASSERT(!m_stop);
    m_queue.push_back(JobItem{function, job_data});
    ++m_outstanding;
  }
  m_wake_cv.notify_one();
}

void JobSystem::wait() {
  ASSERT(m_initialized);
  assertOwner();
  CpuZoneScope job_zone(g_runtime_global_context.m_frame_timing.get(), "Job");
  ZoneScopedN("Job");
  for (;;) {
    JobItem item{};
    {
      std::unique_lock lock(m_mutex);
      if (m_outstanding == 0) {
        return;
      }
      if (m_queue.empty()) {
        m_done_cv.wait(lock, [this]() { return m_outstanding == 0; });
        return;
      }
      item = m_queue.front();
      m_queue.pop_front();
    }
    runJob(item);
  }
}

void JobSystem::parallelFor(uint32_t begin, uint32_t end,
                            ParallelForFunction function, void* job_data) {
  ASSERT(m_initialized);
  assertOwner();
  ASSERT(function != nullptr);
  if (begin >= end) {
    return;
  }
  const uint32_t total = end - begin;
  uint32_t chunk_count = m_dedicated_worker_count + 1;
  if (chunk_count > total) {
    chunk_count = total;
  }
  m_range_jobs.clear();
  m_range_jobs.resize(chunk_count);
  const uint32_t base = total / chunk_count;
  const uint32_t remainder = total % chunk_count;
  uint32_t cursor = begin;
  for (uint32_t i = 0; i < chunk_count; ++i) {
    const uint32_t length = base + (i < remainder ? 1u : 0u);
    m_range_jobs[i].function = function;
    m_range_jobs[i].job_data = job_data;
    m_range_jobs[i].begin = cursor;
    m_range_jobs[i].end = cursor + length;
    cursor += length;
  }
  try {
    for (uint32_t i = 0; i < chunk_count; ++i) {
      submit(&runRangeJob, &m_range_jobs[i]);
    }
    wait();
  } catch (...) {
    wait();
    m_range_jobs.clear();
    throw;
  }
  m_range_jobs.clear();
}

void JobSystem::assertOwner() const {
  ASSERT(std::this_thread::get_id() == m_owner_thread);
}

void JobSystem::workerLoop() {
  for (;;) {
    JobItem item{};
    {
      std::unique_lock lock(m_mutex);
      m_wake_cv.wait(lock, [this]() { return m_stop || !m_queue.empty(); });
      if (m_queue.empty()) {
        return;
      }
      item = m_queue.front();
      m_queue.pop_front();
    }
    runJob(item);
  }
}

void JobSystem::runJob(const JobItem& item) {
  try {
    item.function(item.job_data);
  } catch (...) {
    std::abort();
  }
  {
    std::lock_guard lock(m_mutex);
    ASSERT(m_outstanding > 0);
    --m_outstanding;
    if (m_outstanding == 0) {
      m_done_cv.notify_all();
    }
  }
}

void JobSystem::runRangeJob(void* job_data) {
  RangeJob* range = static_cast<RangeJob*>(job_data);
  range->function(range->begin, range->end, range->job_data);
}

}  // namespace Blunder
