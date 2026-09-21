#include "runtime/function/debug/frame_timing_service.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace Blunder {

FrameTimingService::~FrameTimingService() { detachGpu(); }

void FrameTimingService::beginTick(float delta_seconds) {
  m_building = {};
  m_building.cpu_ms = delta_seconds * 1000.0f;
  m_building.gpu_ms = m_pending_gpu.gpu_ms;
  m_building.pass_count = m_pending_gpu.pass_count;
  for (uint32_t i = 0; i < m_pending_gpu.pass_count && i < k_frame_timing_max_passes;
       ++i) {
    m_building.passes[i] = m_pending_gpu.passes[i];
  }
}

void FrameTimingService::addCpuZone(const char* name, float cpu_ms) {
  if (name == nullptr) {
    return;
  }
  for (uint32_t i = 0; i < m_building.cpu_zone_count; ++i) {
    if (std::strcmp(m_building.cpu_zones[i].name, name) == 0) {
      m_building.cpu_zones[i].cpu_ms += cpu_ms;
      return;
    }
  }
  if (m_building.cpu_zone_count >= k_frame_timing_max_cpu_zones) {
    return;
  }
  FrameTimingCpuZone& zone = m_building.cpu_zones[m_building.cpu_zone_count++];
  setFrameTimingName(zone.name, sizeof(zone.name), name);
  zone.cpu_ms = cpu_ms;
}

void FrameTimingService::setCounts(uint32_t instance_count, uint32_t light_count,
                                   uint32_t draw_count) {
  m_building.instance_count = instance_count;
  m_building.light_count = light_count;
  m_building.draw_count = draw_count;
#ifdef TRACY_ENABLE
  TracyPlot("instances", static_cast<int64_t>(instance_count));
  TracyPlot("lights", static_cast<int64_t>(light_count));
  TracyPlot("draws", static_cast<int64_t>(draw_count));
#endif
}

void FrameTimingService::endTick(int fps) {
  m_building.fps = static_cast<float>(fps);
  m_ring.push(m_building);
}

void FrameTimingService::attachGpu(VulkanContext* context) {
  detachGpu();
  if (context == nullptr) {
    return;
  }
  m_gpu.initialize(context);
  const char* name = context->physicalDeviceName();
  if (name != nullptr) {
    std::strncpy(m_device_name, name, sizeof(m_device_name) - 1);
    m_device_name[sizeof(m_device_name) - 1] = '\0';
  }
  m_gpu_unreliable =
      context->physicalDeviceType() != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
  createTracyVulkan(context);
}

void FrameTimingService::detachGpu() {
#ifdef TRACY_ENABLE
  if (m_tracy_vk != nullptr) {
    TracyVkDestroy(m_tracy_vk);
    m_tracy_vk = nullptr;
  }
#endif
  m_gpu.shutdown();
}

void FrameTimingService::beginGpuSlot(VkCommandBuffer command_buffer,
                                      uint32_t slot) {
  m_gpu.setRecordingSlot(slot);
  m_gpu.resetSlot(command_buffer, slot);
}

void FrameTimingService::harvestGpuSlot(uint32_t slot) {
  m_pending_gpu = {};
  m_gpu.harvestSlot(slot, m_pending_gpu);
}

void FrameTimingService::collectTracy(VkCommandBuffer command_buffer) {
#ifdef TRACY_ENABLE
  BLUNDER_TRACY_VK_COLLECT(m_tracy_vk, command_buffer);
#else
  (void)command_buffer;
#endif
}

void FrameTimingService::createTracyVulkan(VulkanContext* context) {
#ifdef TRACY_ENABLE
  if (context == nullptr || context->getDevice() == VK_NULL_HANDLE) {
    return;
  }
  VkCommandBuffer cmd = context->beginImmediateCommands();
  if (cmd == VK_NULL_HANDLE) {
    LOG_WARN("[FrameTimingService] Tracy Vulkan context skipped: no command buffer");
    return;
  }
  if (context->calibratedTimestampsEnabled()) {
    auto gpdctd = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT>(
        vkGetInstanceProcAddr(context->getInstance(),
                              "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"));
    auto gct = reinterpret_cast<PFN_vkGetCalibratedTimestampsEXT>(
        vkGetDeviceProcAddr(context->getDevice(), "vkGetCalibratedTimestampsEXT"));
    if (gpdctd != nullptr && gct != nullptr) {
      m_tracy_vk = TracyVkContextCalibrated(
          context->getPhysicalDevice(), context->getDevice(),
          context->getGraphicsQueue(), cmd, gpdctd, gct);
    }
  }
  if (m_tracy_vk == nullptr) {
    m_tracy_vk = TracyVkContext(context->getPhysicalDevice(), context->getDevice(),
                                context->getGraphicsQueue(), cmd);
  }
  context->endImmediateCommands(cmd);
  if (m_tracy_vk != nullptr) {
    TracyVkContextName(m_tracy_vk, "Blunder", 7);
  }
#else
  (void)context;
#endif
}

CpuZoneScope::CpuZoneScope(FrameTimingService* service, const char* name)
    : m_service(service), m_name(name), m_start_ns(SDL_GetTicksNS()) {}

CpuZoneScope::~CpuZoneScope() {
  if (m_service == nullptr || m_name == nullptr) {
    return;
  }
  const uint64_t now = SDL_GetTicksNS();
  const float ms = static_cast<float>(now - m_start_ns) * 1.0e-6f;
  m_service->addCpuZone(m_name, ms);
}

}  // namespace Blunder
