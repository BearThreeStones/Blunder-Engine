#include "runtime/function/ui/viewport/ui_viewport_bridge.h"

#include <cstring>

#include <vulkan/vulkan.h>

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
    if (slot.staging) {
      slot.staging->destroy();
      slot.staging.reset();
    }
    slot.cpu_pixels.clear();
    slot.pending_gpu = false;
    slot.has_cpu_frame = false;
  }
  m_has_display_frame = false;
  m_context = nullptr;
  m_allocator = nullptr;
  m_sync = nullptr;
}

void UIViewportBridge::resizeReadback(uint32_t width, uint32_t height) {
  if (!m_allocator || width == 0 || height == 0) {
    return;
  }

  for (ReadbackSlot& slot : m_slots) {
    if (slot.staging) {
      slot.staging->destroy();
      slot.staging.reset();
    }
    slot.pending_gpu = false;
    slot.has_cpu_frame = false;
  }
  m_has_display_frame = false;

  const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * 4u;
  for (ReadbackSlot& slot : m_slots) {
    slot.staging = eastl::make_unique<VulkanBuffer>();
    slot.staging->create(m_allocator, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VMA_MEMORY_USAGE_GPU_TO_CPU);
    slot.cpu_pixels.resize(static_cast<size_t>(bytes));
    slot.width = width;
    slot.height = height;
  }
}

void UIViewportBridge::waitForRecordingSlot(const uint32_t slot) {
  if (!m_context || !m_sync || slot >= k_frames_in_flight) {
    return;
  }
  VkDevice device = m_context->getDevice();
  VkFence fence = m_sync->getInFlightFence(slot);
  vkWaitForFences(device, 1, &fence, VK_TRUE, k_fence_wait_timeout_ns);
  vkResetFences(device, 1, &fence);
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

  const size_t bytes = static_cast<size_t>(width) * height * 4u;
  if (readback_slot.cpu_pixels.size() < bytes) {
    readback_slot.cpu_pixels.resize(bytes);
  }
}

void UIViewportBridge::tryMapSlot(const uint32_t slot) {
  if (!m_context || !m_allocator || !m_sync || slot >= k_frames_in_flight) {
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

  void* mapped = nullptr;
  const VkResult map_result = vmaMapMemory(
      m_allocator->getAllocator(), readback_slot.staging->getAllocation(), &mapped);
  if (map_result != VK_SUCCESS || mapped == nullptr) {
    return;
  }

  const size_t bytes =
      static_cast<size_t>(readback_slot.width) * readback_slot.height * 4u;
  if (readback_slot.cpu_pixels.size() < bytes) {
    readback_slot.cpu_pixels.resize(bytes);
  }
  std::memcpy(readback_slot.cpu_pixels.data(), mapped, bytes);
  vmaUnmapMemory(m_allocator->getAllocator(), readback_slot.staging->getAllocation());

  readback_slot.pending_gpu = false;
  readback_slot.has_cpu_frame = true;
  m_display_slot = slot;
  m_has_display_frame = true;
}

void UIViewportBridge::pollAndPresent(IViewportSink* sink) {
  for (uint32_t slot = 0; slot < k_frames_in_flight; ++slot) {
    tryMapSlot(slot);
  }

  if (!sink || !m_has_display_frame) {
    return;
  }

  const ReadbackSlot& display = m_slots[m_display_slot];
  if (!display.has_cpu_frame || display.width == 0 || display.height == 0) {
    return;
  }

  ViewportCpuFrame frame{};
  frame.pixels_rgba = display.cpu_pixels.data();
  frame.width = display.width;
  frame.height = display.height;
  frame.stride_bytes = display.width * 4u;
  sink->presentViewportCpuFrame(frame);
}

}  // namespace Blunder
