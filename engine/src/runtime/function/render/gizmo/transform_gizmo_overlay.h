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

  void initialize(const OverlayResources& res, SlangCompiler* compiler);
  void shutdown();

  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_screen(VkCommandBuffer cmd, const OverlayState& state) override;

  TransformGizmoController& controller() { return m_controller; }
  const TransformGizmoController& controller() const { return m_controller; }

 private:
  bool drawTranslateHandle(VkCommandBuffer cmd, const OverlayState& state,
                           ManipulatorAxis axis, const GizmoBasis& basis, const float idot[3],
                           float group_scale, bool highlight, float alpha_scale = 1.0f);
  void drawTranslateOriginDot(VkCommandBuffer cmd, const OverlayState& state,
                              const GizmoBasis& basis, float group_scale,
                              ManipulatorAxis color_axis);
  bool drawScaleHandle(VkCommandBuffer cmd, const OverlayState& state, ManipulatorAxis axis,
                       const GizmoBasis& basis, const float idot[3], float group_scale,
                       bool highlight);
  void drawRotationDial(VkCommandBuffer cmd, const OverlayState& state,
                        ManipulatorAxis axis, const GizmoBasis& basis, const float idot[3],
                        float group_scale, bool highlight, bool ghost);
  void drawRotateOuterRing(VkCommandBuffer cmd, const OverlayState& state,
                           const GizmoBasis& basis, float group_scale);
  void drawRotateTrackball(VkCommandBuffer cmd, const OverlayState& state,
                           const GizmoBasis& basis, float group_scale);
  bool drawScalePlaneHandle(VkCommandBuffer cmd, const OverlayState& state,
                            ManipulatorAxis axis, const GizmoBasis& basis, const float idot[3],
                            float group_scale, bool highlight);
  void drawScaleOuterAnnulus(VkCommandBuffer cmd, const OverlayState& state,
                             const GizmoBasis& basis, float group_scale);
  void recordGizmoDraw(VkCommandBuffer cmd, const OverlayState& state);
  void recordDraw(VkCommandBuffer cmd, const OverlayState& state,
                  const glm::mat4& gizmo_world, const glm::vec4& color,
                  GizmoDrawStyle style, float alpha, const glm::vec4& quad_layout,
                  float quad_z, float line_width_scale = 1.0f, float arc_start = 0.0f,
                  float arc_delta = 0.0f,
                  const glm::vec4& clip_plane = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
                  float clip_enabled = 0.0f,
                  ManipulatorAxis axis = ManipulatorAxis::trans_x,
                  float line_width_px = 0.0f,
                  uint32_t dial_sides = TransformGizmoMetrics::k_dial_sides_min);

  TransformGizmoController m_controller;
  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_pipeline;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_buffers;
  uintptr_t m_descriptor_pool{0};
  eastl::vector<uintptr_t> m_descriptor_sets;
  uint32_t m_next_draw_slot{0};
};

}  // namespace Blunder
