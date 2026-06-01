#pragma once

#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "runtime/function/render/gizmo/transform_gizmo_controller.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
#include "runtime/function/render/overlay/overlay_base.h"

namespace Blunder {

class OffscreenRenderTarget;
class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;

struct OverlayResources;
struct OverlayState;

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}  // namespace vulkan_backend

class TransformGizmoOverlay final : public Overlay {
 public:
  TransformGizmoOverlay() = default;
  ~TransformGizmoOverlay();

  void initialize(const OverlayResources& res, SlangCompiler* compiler,
                  OffscreenRenderTarget* offscreen_target);
  void shutdown();

  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_color_only(VkCommandBuffer cmd, const OverlayState& state) override;

  TransformGizmoController& controller() { return m_controller; }
  const TransformGizmoController& controller() const { return m_controller; }

 private:
  void drawTranslateHandle(VkCommandBuffer cmd, const OverlayState& state,
                           ManipulatorAxis axis, const GizmoBasis& basis, float scale,
                           bool highlight);
  void drawRotationDial(VkCommandBuffer cmd, const OverlayState& state,
                        ManipulatorAxis axis, const GizmoBasis& basis, float scale,
                        bool highlight, bool ghost);
  void recordDraw(VkCommandBuffer cmd, const OverlayState& state,
                  const glm::mat4& gizmo_world, const glm::vec4& color,
                  GizmoDrawStyle style, float alpha, float arc_start = 0.0f,
                  float arc_delta = 0.0f);

  TransformGizmoController m_controller;
  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_pipeline;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_buffers;
  uintptr_t m_descriptor_pool{0};
  eastl::vector<uintptr_t> m_descriptor_sets;
};

}  // namespace Blunder
