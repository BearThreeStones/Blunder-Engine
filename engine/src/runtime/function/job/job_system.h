#pragma once

#include "EASTL/deque.h"
#include "EASTL/vector.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace Blunder {

using JobFunction = void (*)(void* job_data);
using ParallelForFunction = void (*)(uint32_t begin, uint32_t end,
                                     void* job_data);

class JobSystem final {
 public:
  JobSystem() = default;
  ~JobSystem();

  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
  JobSystem(JobSystem&&) = delete;
  JobSystem& operator=(JobSystem&&) = delete;

  static uint32_t defaultDedicatedWorkerCount();

  void initialize();
  void initialize(uint32_t dedicated_worker_count);
  void shutdown();

  void submit(JobFunction function, void* job_data);
  void wait();
  void parallelFor(uint32_t begin, uint32_t end, ParallelForFunction function,
                   void* job_data);

  uint32_t dedicatedWorkerCount() const { return m_dedicated_worker_count; }
  bool isInitialized() const { return m_initialized; }

 private:
  struct JobItem {
    JobFunction function{nullptr};
    void* job_data{nullptr};
  };

  struct RangeJob {
    ParallelForFunction function{nullptr};
    void* job_data{nullptr};
    uint32_t begin{0};
    uint32_t end{0};
  };

  void assertOwner() const;
  void joinWorkers();
  void workerLoop();
  void runJob(const JobItem& item);
  static void runRangeJob(void* job_data);

  std::mutex m_mutex;
  std::condition_variable m_wake_cv;
  std::condition_variable m_done_cv;
  eastl::deque<JobItem> m_queue;
  eastl::vector<RangeJob> m_range_jobs;
  std::vector<std::thread> m_workers;
  std::thread::id m_owner_thread{};
  uint32_t m_dedicated_worker_count{0};
  uint32_t m_outstanding{0};
  bool m_stop{false};
  bool m_initialized{false};
};

}  // namespace Blunder
