#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "runtime/function/debug/frame_timing_ring.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder {

class VulkanContext;

enum class GpuTimestampZone : uint8_t {
  viewport_gbuffer = 0,
  viewport_lighting,
  viewport_scene,
  viewport_ssao,
  viewport_volumetric_fog,
  viewport_copy,
  internal_shadow,
  internal_cull,
  internal_froxel,
  internal_lighting_triangle,
  count
};

inline constexpr uint32_t k_gpu_timestamp_zone_count =
    static_cast<uint32_t>(GpuTimestampZone::count);

const char* gpuTimestampZoneName(GpuTimestampZone zone);

class GpuTimestampQueries final {
 public:
  GpuTimestampQueries() = default;
  ~GpuTimestampQueries();

  GpuTimestampQueries(const GpuTimestampQueries&) = delete;
  GpuTimestampQueries& operator=(const GpuTimestampQueries&) = delete;

  void initialize(VulkanContext* context);
  void shutdown();
  void waitDeviceIdle() const;
  bool isInitialized() const { return m_pool != VK_NULL_HANDLE; }

  void resetSlot(VkCommandBuffer command_buffer, uint32_t slot);
  void writeBegin(VkCommandBuffer command_buffer, GpuTimestampZone zone);
  void writeEnd(VkCommandBuffer command_buffer, GpuTimestampZone zone);
  void harvestSlot(uint32_t slot, FrameTimingSlot& dest);

  void setRecordingSlot(uint32_t slot) { m_recording_slot = slot; }

 private:
  uint32_t queryIndex(uint32_t slot, GpuTimestampZone zone, bool end) const;
  void markWritten(GpuTimestampZone zone);

  VulkanContext* m_context{nullptr};
  VkQueryPool m_pool{VK_NULL_HANDLE};
  float m_timestamp_period_ns{1.0f};
  uint64_t m_timestamp_mask{~0ull};
  uint32_t m_recording_slot{0};
  uint32_t m_written_mask[VulkanSync::k_max_frames_in_flight]{};
};

class GpuPassScope final {
 public:
  GpuPassScope(GpuTimestampQueries* queries, VkCommandBuffer command_buffer,
               GpuTimestampZone zone)
      : m_queries(queries), m_command_buffer(command_buffer), m_zone(zone) {
    if (m_queries != nullptr) {
      m_queries->writeBegin(m_command_buffer, m_zone);
    }
  }
  ~GpuPassScope() {
    if (m_queries != nullptr) {
      m_queries->writeEnd(m_command_buffer, m_zone);
    }
  }
  GpuPassScope(const GpuPassScope&) = delete;
  GpuPassScope& operator=(const GpuPassScope&) = delete;

 private:
  GpuTimestampQueries* m_queries{nullptr};
  VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
  GpuTimestampZone m_zone{GpuTimestampZone::viewport_copy};
};

}  // namespace Blunder
