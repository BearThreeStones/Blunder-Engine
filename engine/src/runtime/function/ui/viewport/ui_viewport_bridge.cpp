#include "runtime/function/ui/viewport/ui_viewport_bridge.h"

#include <vulkan/vulkan.h>

#include <cstdint>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/ui/viewport/i_viewport_sink.h"
#include "runtime/function/ui/viewport/viewport_cpu_frame.h"

namespace Blunder {

namespace {

constexpr uint64_t k_fence_wait_timeout_ns = 1'000'000'000ull;

}  // namespace

UIViewportBridge::UIViewportBridge() = default;

UIViewportBridge::~UIViewportBridge() { shutdown(); }

void UIViewportBridge::initialize(VulkanContext* context, VulkanAllocator* allocator,
                                  VulkanSync* sync) {
  ASSERT(context);
  ASSERT(allocator);
  ASSERT(sync);
  m_context = context;
  m_allocator = allocator;
  m_sync = sync;
}

void UIViewportBridge::shutdown() {
  if (m_context) {
    vkDeviceWaitIdle(m_context->getDevice());
  }
  for (ReadbackSlot& slot : m_slots) {
    if (slot.persistent_map && slot.staging && m_allocator) {
      vmaUnmapMemory(m_allocator->getAllocator(),
                     slot.staging->getAllocation());
      slot.persistent_map = nullptr;
    }
    if (slot.staging) {
      slot.staging->destroy();
      slot.staging.reset();
    }
    slot.pending_gpu = false;
    slot.has_cpu_frame = false;
    slot.completed_generation = 0;
  }
  m_has_display_frame = false;
  m_last_presented_to_sink_generation = 0;
  m_next_completed_generation = 1;
  m_context = nullptr;
  m_allocator = nullptr;
  m_sync = nullptr;
}

void UIViewportBridge::resizeReadback(uint32_t width, uint32_t height) {
  if (!m_allocator || width == 0 || height == 0) {
    return;
  }

  for (ReadbackSlot& slot : m_slots) {
    if (slot.persistent_map && slot.staging) {
      vmaUnmapMemory(m_allocator->getAllocator(),
                     slot.staging->getAllocation());
      slot.persistent_map = nullptr;
    }
    if (slot.staging) {
      slot.staging->destroy();
      slot.staging.reset();
    }
    slot.pending_gpu = false;
    slot.has_cpu_frame = false;
    slot.completed_generation = 0;
  }
  m_has_display_frame = false;
  m_last_presented_to_sink_generation = 0;
  m_next_completed_generation = 1;

  const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * 4u;
  for (ReadbackSlot& slot : m_slots) {
    slot.staging = eastl::make_unique<VulkanBuffer>();
    slot.staging->create(m_allocator, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VMA_MEMORY_USAGE_GPU_TO_CPU);
    // Persistently map the staging buffer to avoid per-frame map/unmap overhead.
    const VkResult map_result = vmaMapMemory(
        m_allocator->getAllocator(), slot.staging->getAllocation(),
        &slot.persistent_map);
    if (map_result != VK_SUCCESS) {
      LOG_WARN(
          "[UIViewportBridge] persistent vmaMapMemory failed ({}); "
          "falling back to per-frame mapping",
          static_cast<int>(map_result));
      slot.persistent_map = nullptr;
    }
    slot.width = width;
    slot.height = height;
  }
}

void UIViewportBridge::waitForRecordingSlot(const uint32_t slot) {
  if (!tryBeginRecordingSlot(slot)) {
    if (!m_context || !m_sync || slot >= k_frames_in_flight) {
      return;
    }
    VkDevice device = m_context->getDevice();
    VkFence fence = m_sync->getInFlightFence(slot);
    vkWaitForFences(device, 1, &fence, VK_TRUE, k_fence_wait_timeout_ns);
    vkResetFences(device, 1, &fence);
  }
}

bool UIViewportBridge::tryBeginRecordingSlot(const uint32_t slot) {
  if (!m_context || !m_sync || slot >= k_frames_in_flight) {
    return false;
  }
  VkDevice device = m_context->getDevice();
  VkFence fence = m_sync->getInFlightFence(slot);
  const VkResult fence_status = vkGetFenceStatus(device, fence);
  if (fence_status == VK_NOT_READY) {
    return false;
  }
  if (fence_status != VK_SUCCESS) {
    vkWaitForFences(device, 1, &fence, VK_TRUE, k_fence_wait_timeout_ns);
  }
  vkResetFences(device, 1, &fence);
  return true;
}

VulkanBuffer* UIViewportBridge::stagingBuffer(const uint32_t slot) {
  if (slot >= k_frames_in_flight) {
    return nullptr;
  }
  return m_slots[slot].staging.get();
}

void UIViewportBridge::notifyGpuSubmitted(const uint32_t slot, const uint32_t width,
                                          const uint32_t height) {
  if (slot >= k_frames_in_flight) {
    return;
  }
  ReadbackSlot& readback_slot = m_slots[slot];
  readback_slot.width = width;
  readback_slot.height = height;
  readback_slot.pending_gpu = true;
}

void UIViewportBridge::tryMapSlot(const uint32_t slot) {
  if (!m_context || !m_sync || slot >= k_frames_in_flight) {
    return;
  }

  ReadbackSlot& readback_slot = m_slots[slot];
  if (!readback_slot.pending_gpu || !readback_slot.staging) {
    return;
  }

  VkFence fence = m_sync->getInFlightFence(slot);
  const VkResult fence_status = vkGetFenceStatus(m_context->getDevice(), fence);
  if (fence_status != VK_SUCCESS) {
    return;
  }

  readback_slot.pending_gpu = false;
  readback_slot.has_cpu_frame = true;
  readback_slot.completed_generation = m_next_completed_generation++;
  m_has_display_frame = true;
}

void UIViewportBridge::pollAndPresent(IViewportSink* sink) {
  for (uint32_t slot = 0; slot < k_frames_in_flight; ++slot) {
    tryMapSlot(slot);
  }

  if (!sink || !m_has_display_frame) {
    return;
  }

  uint32_t best_slot = UINT32_MAX;
  uint64_t best_generation = 0;
  for (uint32_t slot = 0; slot < k_frames_in_flight; ++slot) {
    const ReadbackSlot& readback_slot = m_slots[slot];
    if (!readback_slot.has_cpu_frame || readback_slot.width == 0 ||
        readback_slot.height == 0) {
      continue;
    }
    if (readback_slot.completed_generation <= m_last_presented_to_sink_generation) {
      continue;
    }
    if (readback_slot.completed_generation > best_generation) {
      best_generation = readback_slot.completed_generation;
      best_slot = slot;
    }
  }

  if (best_slot == UINT32_MAX) {
    return;
  }

  const ReadbackSlot& display = m_slots[best_slot];
  m_display_slot = best_slot;

  const uint8_t* pixels = nullptr;
  if (display.persistent_map) {
    // Fast path: read directly from the persistently mapped staging buffer.
    pixels = static_cast<const uint8_t*>(display.persistent_map);
  } else if (m_allocator && display.staging) {
    // Fallback: one-shot map when persistent mapping was unavailable.
    void* mapped = nullptr;
    const VkResult map_result = vmaMapMemory(
        m_allocator->getAllocator(), display.staging->getAllocation(), &mapped);
    if (map_result != VK_SUCCESS || mapped == nullptr) {
      return;
    }
    pixels = static_cast<const uint8_t*>(mapped);
  }
  if (!pixels) {
    return;
  }

  ViewportCpuFrame frame{};
  frame.pixels_rgba = pixels;
  frame.width = display.width;
  frame.height = display.height;
  frame.stride_bytes = display.width * 4u;
  sink->presentViewportCpuFrame(frame);
  m_last_presented_to_sink_generation = best_generation;

  // Unmap only if we used the one-shot fallback path.
  if (!display.persistent_map && m_allocator && display.staging) {
    vmaUnmapMemory(m_allocator->getAllocator(),
                   display.staging->getAllocation());
  }
}

}  // namespace Blunder
