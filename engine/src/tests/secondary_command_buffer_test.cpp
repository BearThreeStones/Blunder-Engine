#include "runtime/function/render/vulkan/secondary_command_buffer_pool.h"

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

  constexpr uint32_t k_slot_count =
      SecondaryCommandBufferPool::k_stream_count *
      SecondaryCommandBufferPool::k_pass_count *
      VulkanSync::k_max_frames_in_flight;

  SecondaryCommandBufferPool pool;
  pool.initialize(nullptr);
  expect_true("no-device initialize allocates nothing", !pool.isAllocated());

  bool seen[k_slot_count]{};
  uint32_t unique_count = 0;
  for (uint32_t stream = 0; stream < SecondaryCommandBufferPool::k_stream_count;
       ++stream) {
    for (uint32_t pass = 0; pass < SecondaryCommandBufferPool::k_pass_count;
         ++pass) {
      for (uint32_t frame = 0; frame < VulkanSync::k_max_frames_in_flight;
           ++frame) {
        const uint32_t index = SecondaryCommandBufferPool::slotIndex(
            static_cast<SecondaryStream>(stream),
            static_cast<SecondaryPass>(pass), frame);
        expect_true("slot index in range", index < k_slot_count);
        if (index < k_slot_count && !seen[index]) {
          seen[index] = true;
          ++unique_count;
        } else if (index < k_slot_count) {
          expect_true("slot indexes do not collide", false);
        }
      }
    }
  }
  expect_true("every stream/pass/frame has a unique slot",
              unique_count == k_slot_count);

  const uint32_t viewport = SecondaryCommandBufferPool::slotIndex(
      SecondaryStream::viewport, SecondaryPass::forward_opaque, 0);
  const uint32_t camera_preview = SecondaryCommandBufferPool::slotIndex(
      SecondaryStream::camera_preview, SecondaryPass::forward_opaque, 0);
  const uint32_t immediate = SecondaryCommandBufferPool::slotIndex(
      SecondaryStream::immediate, SecondaryPass::forward_opaque, 0);
  expect_true("viewport and camera preview slots differ",
              viewport != camera_preview);
  expect_true("viewport and immediate slots differ", viewport != immediate);
  expect_true("camera preview and immediate slots differ",
              camera_preview != immediate);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
