#include "runtime/function/job/job_system.h"

#include <cstdio>
#include <thread>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

struct SlotWrite {
  int* slots;
  uint32_t index;
  int value;
};

void writeSlot(void* job_data) {
  SlotWrite* write = static_cast<SlotWrite*>(job_data);
  write->slots[write->index] = write->value;
}

void fillSlots(uint32_t begin, uint32_t end, void* job_data) {
  int* slots = static_cast<int*>(job_data);
  for (uint32_t i = begin; i < end; ++i) {
    slots[i] = static_cast<int>(i + 1);
  }
}

void runBufferBatch(Blunder::JobSystem& jobs, const char* label) {
  constexpr uint32_t k_count = 8;
  int slots[k_count];
  SlotWrite writes[k_count];
  for (uint32_t i = 0; i < k_count; ++i) {
    slots[i] = 0;
    writes[i].slots = slots;
    writes[i].index = i;
    writes[i].value = static_cast<int>(i + 1);
    jobs.submit(&writeSlot, &writes[i]);
  }
  jobs.wait();
  bool ok = true;
  for (uint32_t i = 0; i < k_count; ++i) {
    if (slots[i] != static_cast<int>(i + 1)) {
      ok = false;
      break;
    }
  }
  expect_true(label, ok);
}

void runEdgeCases(Blunder::JobSystem& jobs) {
  jobs.wait();
  expect_true("wait with no jobs returns", jobs.isInitialized());

  int sentinel[4] = {7, 7, 7, 7};
  jobs.parallelFor(2, 2, &fillSlots, sentinel);
  jobs.parallelFor(3, 1, &fillSlots, sentinel);
  expect_true("empty and inverted parallelFor are no-ops",
              sentinel[0] == 7 && sentinel[3] == 7);

  int slots[6] = {};
  jobs.parallelFor(2, 6, &fillSlots, slots);
  expect_true("offset parallelFor writes [begin,end)",
              slots[0] == 0 && slots[1] == 0 && slots[2] == 3 && slots[5] == 6);
}

void runParallelFor(Blunder::JobSystem& jobs, const char* label) {
  constexpr uint32_t k_count = 8;
  int slots[k_count];
  for (uint32_t i = 0; i < k_count; ++i) {
    slots[i] = 0;
  }
  jobs.parallelFor(0, k_count, &fillSlots, slots);
  bool ok = true;
  for (uint32_t i = 0; i < k_count; ++i) {
    if (slots[i] != static_cast<int>(i + 1)) {
      ok = false;
      break;
    }
  }
  expect_true(label, ok);
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    JobSystem jobs;
    jobs.initialize(0);
    expect_true("zero workers initialized", jobs.isInitialized());
    expect_true("zero dedicated workers", jobs.dedicatedWorkerCount() == 0);
    runBufferBatch(jobs, "zero workers buffer batch");
    runParallelFor(jobs, "zero workers parallelFor");
    runEdgeCases(jobs);
    jobs.shutdown();
    expect_true("zero workers shutdown", !jobs.isInitialized());
  }

  {
    JobSystem jobs;
    jobs.initialize();
    const uint32_t expected = JobSystem::defaultDedicatedWorkerCount();
    expect_true("default worker count", jobs.dedicatedWorkerCount() == expected);
    const unsigned hardware = std::thread::hardware_concurrency();
    const uint32_t formula = hardware == 0 ? 0 : hardware - 1;
    expect_true("default matches N-1", expected == formula);
    runBufferBatch(jobs, "default workers buffer batch");
    runParallelFor(jobs, "default workers parallelFor");
    runEdgeCases(jobs);
    jobs.shutdown();
    expect_true("default workers shutdown", !jobs.isInitialized());
  }

  {
    JobSystem jobs;
    jobs.initialize(0);
    int slot = 0;
    SlotWrite write{&slot, 0, 42};
    jobs.submit(&writeSlot, &write);
    jobs.shutdown();
    expect_true("shutdown drains outstanding",
                slot == 42 && !jobs.isInitialized());
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
