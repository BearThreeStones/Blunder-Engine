#pragma once

#include <cstdint>

#include "EASTL/array.h"
#include "EASTL/vector.h"

#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder {

class IViewportSink;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class VulkanSync;

/// Schedules Vulkan offscreen readback with in-flight fences (no post-submit stall).
class UIViewportBridge final {
 public:
  UIViewportBridge();
  ~UIViewportBridge();

  void initialize(VulkanContext* context, VulkanAllocator* allocator,
                    VulkanSync* sync);
  void shutdown();

  void resizeReadback(uint32_t width, uint32_t height);

  /// Waits for the slot fence and resets it (call before recording the slot CB).
  void waitForRecordingSlot(uint32_t slot);

  VulkanBuffer* stagingBuffer(uint32_t slot);

  /// Call after `vkQueueSubmit` for this slot's in-flight fence.
  void notifyGpuSubmitted(uint32_t slot, uint32_t width, uint32_t height);

  /// Maps any signaled slots and presents the newest completed frame to the sink.
  void pollAndPresent(IViewportSink* sink);

  static constexpr uint32_t k_frames_in_flight = VulkanSync::k_max_frames_in_flight;

 private:
  struct ReadbackSlot {
    eastl::unique_ptr<VulkanBuffer> staging;
    eastl::vector<uint8_t> cpu_pixels;
    uint32_t width{0};
    uint32_t height{0};
    bool pending_gpu{false};
    bool has_cpu_frame{false};
  };

  void tryMapSlot(uint32_t slot);

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  VulkanSync* m_sync{nullptr};
  eastl::array<ReadbackSlot, k_frames_in_flight> m_slots{};
  uint32_t m_display_slot{0};
  bool m_has_display_frame{false};
};

}  // namespace Blunder
