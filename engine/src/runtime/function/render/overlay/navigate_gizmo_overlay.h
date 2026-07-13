#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/overlay/navigate_gizmo_layout.h"
#include "runtime/function/render/overlay/overlay_base.h"

namespace Blunder {

class EditorCamera;
class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;

struct OverlayResources;
struct OverlayState;

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}  // namespace vulkan_backend

/// Blender-style 3D navigation gizmo (orientation indicator).
///
/// Renders in ScreenOverlayPass after post-processing so SSAO composite does not
/// clear gizmo pixels from the offscreen color target.
class NavigateGizmoOverlay final : public Overlay {
 public:
  NavigateGizmoOverlay() = default;
  ~NavigateGizmoOverlay();

  void initialize(const OverlayResources& res, SlangCompiler* compiler);
  void shutdown();

  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_screen(VkCommandBuffer cmd, const OverlayState& state) override;

  /// Returns true when the click hit a nav gizmo axis (view snap).
  bool tryHandleMouseClick(const Vec2& window_position, EditorCamera& camera);

  /// Updates hover highlight; returns true when hover state changed.
  bool updateHoverFromPointer(const Vec2& window_position, const EditorCamera& camera);
  void clearHover();
  bool hasHover() const { return m_gizmo_active; }

 private:
  void record_gizmo_draw(VkCommandBuffer cmd, const OverlayState& state,
                         float gizmo_x, float gizmo_y, float gizmo_size);

  bool m_gizmo_active{false};
  int m_highlight_endpoint{-1};

  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};

  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_pipeline;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_buffers;
  uintptr_t m_descriptor_pool{0};
  eastl::vector<uintptr_t> m_descriptor_sets;
};

}  // namespace Blunder
