#include "runtime/function/debug/frame_timing_ring.h"

#ifdef TRACY_ENABLE
#error "frame_timing_ring_test must compile without TRACY_ENABLE (CI / default)"
#endif

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_near(const char* label, float value, float expected, float eps) {
  const float delta = value > expected ? value - expected : expected - value;
  expect_true(label, delta <= eps);
}

}  // namespace

int main() {
  using Blunder::FrameTimingRing;
  using Blunder::FrameTimingSlot;
  using Blunder::k_frame_timing_ring_capacity;
  using Blunder::setFrameTimingName;

  FrameTimingRing ring;
  expect_true("empty size is 0", ring.size() == 0);
  expect_true("empty latest cpu is 0", ring.latest().cpu_ms == 0.0f);

  FrameTimingSlot first{};
  first.cpu_ms = 16.0f;
  first.gpu_ms = 4.0f;
  first.fps = 60.0f;
  first.pass_count = 1;
  setFrameTimingName(first.passes[0].name, sizeof(first.passes[0].name),
                     "viewport.lighting");
  first.passes[0].gpu_ms = 3.5f;
  ring.push(first);
  expect_true("size after one push", ring.size() == 1);
  expect_near("latest cpu", ring.latest().cpu_ms, 16.0f, 0.01f);
  expect_near("latest pass gpu", ring.latest().passes[0].gpu_ms, 3.5f, 0.01f);

  FrameTimingSlot counts{};
  counts.instance_count = 10927;
  counts.batch_count = 252;
  counts.draw_count = 48001;
  ring.push(counts);
  expect_true("inst is packed instances",
              ring.latest().instance_count == 10927);
  expect_true("batches is MeshBatch count", ring.latest().batch_count == 252);
  expect_true("cmds is surviving indirect not inst",
              ring.latest().draw_count == 48001);
  expect_true("inst != cmds",
              ring.latest().instance_count != ring.latest().draw_count);
  expect_true("batches != inst",
              ring.latest().batch_count != ring.latest().instance_count);

  for (size_t i = 0; i < k_frame_timing_ring_capacity + 17; ++i) {
    FrameTimingSlot slot{};
    slot.cpu_ms = static_cast<float>(i + 1);
    slot.gpu_ms = static_cast<float>(i) * 0.1f;
    ring.push(slot);
  }
  expect_true("wrap keeps capacity", ring.size() == k_frame_timing_ring_capacity);
  expect_near("latest after wrap is last cpu", ring.latest().cpu_ms,
              static_cast<float>(k_frame_timing_ring_capacity + 17), 0.01f);

  FrameTimingSlot copy[k_frame_timing_ring_capacity];
  size_t copied = 0;
  ring.copyChronological(copy, k_frame_timing_ring_capacity, &copied);
  expect_true("copy count", copied == k_frame_timing_ring_capacity);
  expect_near("oldest after wrap", copy[0].cpu_ms, 18.0f, 0.01f);
  expect_near("newest chronological", copy[copied - 1].cpu_ms,
              ring.latest().cpu_ms, 0.01f);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failures\n", g_failures);
    return 1;
  }
  std::printf("frame_timing_ring_test ok (Tracy Client compiled out)\n");
  return 0;
}
