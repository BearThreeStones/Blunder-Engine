#pragma once

#include <atomic>
#include <cstdint>

#include <vulkan/vulkan.h>

#include "runtime/function/debug/frame_timing_ring.h"
#include "runtime/function/debug/gpu_timestamp_queries.h"
#include "runtime/function/debug/tracy_instrument.h"

#ifdef TRACY_ENABLE
#include "runtime/function/debug/tracy_vk_instrument.h"
#endif

namespace Blunder {

class VulkanContext;

class FrameTimingService final {
 public:
  FrameTimingService() = default;
  ~FrameTimingService();

  FrameTimingService(const FrameTimingService&) = delete;
  FrameTimingService& operator=(const FrameTimingService&) = delete;

  void beginTick(float delta_seconds);
  void addCpuZone(const char* name, float cpu_ms);
  void addJobWorkNs(uint64_t ns);
  void setCounts(uint32_t instance_count, uint32_t light_count,
                 uint32_t draw_count);
  void endTick(int fps);

  void attachGpu(VulkanContext* context);
  void detachGpu();
  GpuTimestampQueries* gpuQueries() { return m_gpu.isInitialized() ? &m_gpu : nullptr; }

  void beginGpuSlot(VkCommandBuffer command_buffer, uint32_t slot);
  void harvestGpuSlot(uint32_t slot);
  void collectTracy(VkCommandBuffer command_buffer);

#ifdef TRACY_ENABLE
  TracyVkCtx tracyVk() const { return m_tracy_vk; }
#endif

  const FrameTimingRing& ring() const { return m_ring; }
  bool gpuTimingsUnreliable() const { return m_gpu_unreliable; }
  const char* deviceName() const { return m_device_name; }

 private:
  void createTracyVulkan(VulkanContext* context);

  FrameTimingRing m_ring;
  FrameTimingSlot m_building{};
  FrameTimingSlot m_pending_gpu{};
  GpuTimestampQueries m_gpu;
  std::atomic<uint64_t> m_job_work_ns{0};
  bool m_gpu_unreliable{false};
  char m_device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE]{};
#ifdef TRACY_ENABLE
  TracyVkCtx m_tracy_vk{nullptr};
#endif
};

class CpuZoneScope final {
 public:
  CpuZoneScope(FrameTimingService* service, const char* name);
  ~CpuZoneScope();
  CpuZoneScope(const CpuZoneScope&) = delete;
  CpuZoneScope& operator=(const CpuZoneScope&) = delete;

 private:
  FrameTimingService* m_service{nullptr};
  const char* m_name{nullptr};
  uint64_t m_start_ns{0};
};

}  // namespace Blunder
