#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/function/render/overlay/gizmo_line_command.h"

namespace Blunder {

class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;

struct OverlayResources;
struct OverlayState;

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}  // namespace vulkan_backend

struct DebugDrawBatchStats {
  uint32_t command_count{0};
  uint32_t draw_calls{0};
};

class DebugDrawOverlay final {
 public:
  DebugDrawOverlay() = default;
  ~DebugDrawOverlay();

  void initialize(const OverlayResources& res, SlangCompiler* compiler,
                  VkRenderPass render_pass);
  void shutdown();

  DebugDrawBatchStats draw(VkCommandBuffer cmd, const OverlayState& state);

 private:
  void ensureStorageCapacity(uint32_t frame_slot, uint32_t command_count);
  void updateFrameDescriptors(uint32_t frame_slot);

  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_xray_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_front_pipeline;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_xray;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_front;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_storage_buffers;
  uintptr_t m_descriptor_pool{0};
  eastl::vector<uintptr_t> m_sets_xray;
  eastl::vector<uintptr_t> m_sets_front;
  GizmoLineCommandStream m_stream;
};

}  // namespace Blunder
