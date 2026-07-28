#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/function/render/overlay/overlay_base.h"

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

/// Blender-like wire Camera Gizmo for scene Camera Components.
/// Renders in ScreenOverlayPass (after SSAO) with no depth test.
class CameraGizmoOverlay final : public Overlay {
 public:
  CameraGizmoOverlay() = default;
  ~CameraGizmoOverlay();

  void initialize(const OverlayResources& res, SlangCompiler* compiler);
  void shutdown();

  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_screen(VkCommandBuffer cmd, const OverlayState& state) override;

 private:
  enum class DrawStyle : uint32_t {
    line = 0,
    triangle = 1,
    origin_disc = 2,
  };

  void recordDraw(VkCommandBuffer cmd, const OverlayState& state, DrawStyle style,
                  const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                  const glm::vec4& color);

  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_pipeline;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_buffers;
  uintptr_t m_descriptor_pool{0};
  eastl::vector<uintptr_t> m_descriptor_sets;
  uint32_t m_next_draw_slot{0};
};

}  // namespace Blunder
