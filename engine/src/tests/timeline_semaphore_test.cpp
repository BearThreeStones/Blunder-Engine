#include "runtime/function/render/vulkan/vulkan_sync.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  VulkanSync sync;
  sync.initialize(nullptr, VulkanSync::k_max_frames_in_flight);
  expect_true("no-device initialize allocates nothing",
              !sync.isTimelineAllocated());

  const uint64_t first = sync.issueValue();
  const uint64_t second = sync.issueValue();
  expect_true("issueValue is monotonic", first < second);
  expect_true("first issued value is 1", first == 1);

  sync.setSlotValue(0, first);
  expect_true("slot 0 stores issued value", sync.slotValue(0) == first);
  expect_true("slot 1 still initial", sync.slotValue(1) == 0);
  expect_true("no-device slotReached does not hang", sync.slotReached(0));
  expect_true("no-device waitValue does not hang",
              sync.waitValue(second, 0) == VK_SUCCESS);
  expect_true("no-device waitSlot does not hang",
              sync.waitSlot(0, 0) == VK_SUCCESS);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
