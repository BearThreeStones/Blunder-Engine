#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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

struct GizmoLineBatchStats {
  uint32_t command_count{0};
  uint32_t draw_calls{0};
  bool gpu_upload_skipped{false};
};

/// Batched camera/light overlay wires: command stream → one instanced draw.
class GizmoLineBatch final {
 public:
  GizmoLineBatch() = default;
  ~GizmoLineBatch();

  void initialize(const OverlayResources& res, SlangCompiler* compiler);
  void shutdown();

  void begin();
  void push(GizmoLineDrawStyle style, const glm::vec3& p0, const glm::vec3& p1,
            const glm::vec3& p2, const glm::vec4& color,
            float width_px = 0.0f);

  GizmoLineCommandStream& stream() { return m_stream; }
  const GizmoLineCommandStream& stream() const { return m_stream; }

  GizmoLineBatchStats flush(VkCommandBuffer cmd, const OverlayState& state,
                            float line_width_px);

 private:
  void ensureStorageCapacity(uint32_t frame_slot, uint32_t command_count);
  void updateFrameDescriptors(uint32_t frame_slot);

  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_pipeline;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_storage_buffers;
  uintptr_t m_descriptor_pool{0};
  eastl::vector<uintptr_t> m_descriptor_sets;
  eastl::vector<uint64_t> m_uploaded_hash;
  GizmoLineCommandStream m_stream;
};

}  // namespace Blunder
