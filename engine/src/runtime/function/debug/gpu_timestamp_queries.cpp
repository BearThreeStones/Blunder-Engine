#include "runtime/function/debug/gpu_timestamp_queries.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

namespace {

constexpr uint32_t k_queries_per_slot = k_gpu_timestamp_zone_count * 2u;

const char* k_zone_names[k_gpu_timestamp_zone_count] = {
    "viewport.gbuffer",
    "viewport.lighting",
    "viewport.scene",
    "viewport.ssao",
    "viewport.volumetric_fog",
    "viewport.copy",
    "shadow",
    "cull",
    "froxel",
    "lighting.triangle",
};

}  // namespace

const char* gpuTimestampZoneName(GpuTimestampZone zone) {
  const uint32_t index = static_cast<uint32_t>(zone);
  if (index >= k_gpu_timestamp_zone_count) {
    return "unknown";
  }
  return k_zone_names[index];
}

GpuTimestampQueries::~GpuTimestampQueries() { shutdown(); }

void GpuTimestampQueries::initialize(VulkanContext* context) {
  shutdown();
  if (context == nullptr || context->getDevice() == VK_NULL_HANDLE) {
    return;
  }
  m_context = context;
  m_timestamp_period_ns = context->timestampPeriod();
  if (m_timestamp_period_ns <= 0.0f) {
    m_timestamp_period_ns = 1.0f;
  }
  const uint32_t valid_bits = context->timestampValidBits();
  if (valid_bits > 0 && valid_bits < 64) {
    m_timestamp_mask = (valid_bits == 64) ? ~0ull : ((1ull << valid_bits) - 1ull);
  } else {
    m_timestamp_mask = ~0ull;
  }

  VkQueryPoolCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  create_info.queryCount = k_queries_per_slot * VulkanSync::k_max_frames_in_flight;
  const VkResult result =
      vkCreateQueryPool(context->getDevice(), &create_info, nullptr, &m_pool);
  if (result != VK_SUCCESS) {
    LOG_WARN("[GpuTimestampQueries] vkCreateQueryPool failed: {}",
             static_cast<int>(result));
    m_pool = VK_NULL_HANDLE;
  }
}

void GpuTimestampQueries::waitDeviceIdle() const {
  if (m_context != nullptr && m_context->getDevice() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_context->getDevice());
  }
}

void GpuTimestampQueries::shutdown() {
  if (m_pool != VK_NULL_HANDLE && m_context != nullptr &&
      m_context->getDevice() != VK_NULL_HANDLE) {
    vkDestroyQueryPool(m_context->getDevice(), m_pool, nullptr);
  }
  m_pool = VK_NULL_HANDLE;
  m_context = nullptr;
  for (uint32_t& mask : m_written_mask) {
    mask = 0;
  }
}

uint32_t GpuTimestampQueries::queryIndex(uint32_t slot, GpuTimestampZone zone,
                                         bool end) const {
  return slot * k_queries_per_slot + static_cast<uint32_t>(zone) * 2u +
         (end ? 1u : 0u);
}

void GpuTimestampQueries::markWritten(GpuTimestampZone zone) {
  if (m_recording_slot >= VulkanSync::k_max_frames_in_flight) {
    return;
  }
  m_written_mask[m_recording_slot] |= 1u << static_cast<uint32_t>(zone);
}

void GpuTimestampQueries::resetSlot(VkCommandBuffer command_buffer,
                                    uint32_t slot) {
  if (m_pool == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE ||
      slot >= VulkanSync::k_max_frames_in_flight) {
    return;
  }
  m_recording_slot = slot;
  m_written_mask[slot] = 0;
  vkCmdResetQueryPool(command_buffer, m_pool, slot * k_queries_per_slot,
                      k_queries_per_slot);
  // Reset is a transfer write. Timestamp writes have no implicit wait on it.
  VkMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask =
      VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0,
                       nullptr, 0, nullptr);
}

void GpuTimestampQueries::writeBegin(VkCommandBuffer command_buffer,
                                     GpuTimestampZone zone) {
  if (m_pool == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE) {
    return;
  }
  vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_pool,
                      queryIndex(m_recording_slot, zone, false));
  markWritten(zone);
}

void GpuTimestampQueries::writeEnd(VkCommandBuffer command_buffer,
                                   GpuTimestampZone zone) {
  if (m_pool == VK_NULL_HANDLE || command_buffer == VK_NULL_HANDLE) {
    return;
  }
  vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                      m_pool, queryIndex(m_recording_slot, zone, true));
}

void GpuTimestampQueries::harvestSlot(uint32_t slot, FrameTimingSlot& dest) {
  dest.gpu_ms = 0.0f;
  dest.pass_count = 0;
  if (m_pool == VK_NULL_HANDLE || m_context == nullptr ||
      slot >= VulkanSync::k_max_frames_in_flight) {
    return;
  }
  const uint32_t written = m_written_mask[slot];
  if (written == 0) {
    return;
  }

  float total_ms = 0.0f;
  const uint64_t wrap = m_timestamp_mask + 1ull;
  for (uint32_t z = 0; z < k_gpu_timestamp_zone_count; ++z) {
    if ((written & (1u << z)) == 0) {
      continue;
    }
    // Fetch only this zone's begin/end. A whole-slot vkGetQueryPoolResults
    // returns VK_NOT_READY when any reset-but-unwritten query is still
    // unavailable, which zeroes GPU ms and empties the Pass list.
    uint64_t samples[4]{};
    const uint32_t first = queryIndex(slot, static_cast<GpuTimestampZone>(z), false);
    const VkResult result = vkGetQueryPoolResults(
        m_context->getDevice(), m_pool, first, 2u, sizeof(samples), samples,
        sizeof(uint64_t) * 2u,
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    if (result != VK_SUCCESS && result != VK_NOT_READY) {
      continue;
    }
    if (samples[1] == 0 || samples[3] == 0) {
      continue;
    }
    uint64_t begin = samples[0] & m_timestamp_mask;
    uint64_t end = samples[2] & m_timestamp_mask;
    if (end < begin && wrap > 1ull) {
      end += wrap;
    }
    if (end < begin) {
      continue;
    }
    const float ms = static_cast<float>(end - begin) * m_timestamp_period_ns *
                     1.0e-6f;
    const GpuTimestampZone zone = static_cast<GpuTimestampZone>(z);
    const bool is_pass =
        zone == GpuTimestampZone::viewport_gbuffer ||
        zone == GpuTimestampZone::viewport_lighting ||
        zone == GpuTimestampZone::viewport_scene ||
        zone == GpuTimestampZone::viewport_ssao ||
        zone == GpuTimestampZone::viewport_volumetric_fog ||
        zone == GpuTimestampZone::viewport_copy;
    if (is_pass) {
      total_ms += ms;
    }
    if (dest.pass_count < k_frame_timing_max_passes) {
      FrameTimingPassGpu& row = dest.passes[dest.pass_count++];
      setFrameTimingName(row.name, sizeof(row.name), gpuTimestampZoneName(zone));
      row.gpu_ms = ms;
    }
  }
  dest.gpu_ms = total_ms;
}

}  // namespace Blunder
