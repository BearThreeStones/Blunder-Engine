#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>

#include "EASTL/vector.h"

#include "runtime/function/render/overlay/pick_overlay.h"
#include "runtime/function/render/pick/pick_broad_hits.h"
#include "runtime/function/render/pick/pick_broad_phase.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class EditorCamera;
class RenderSystem;
class SceneInstance;
class SlangCompiler;
class ViewportPickSystem;
class VulkanAllocator;
class VulkanContext;

enum class PickRequestKind : uint8_t {
  single_click = 0,
  multi_peel = 1,
  piercing_menu = 2,
};

enum class PickFetchStatus : uint8_t {
  idle = 0,
  pending = 1,
  ready = 2,
};

struct PickResult {
  PickRequestKind kind{PickRequestKind::single_click};
  float window_x{0.0f};
  float window_y{0.0f};
  uint16_t modifier_flags{0};
  EntityId entity_id{k_invalid_entity_id};
  eastl::vector<EntityId> peel_hits;
};

/// Async GPU viewport pick orchestrator (broad compute + narrow raster).
class HybridGpuPickSystem final {
 public:
  static constexpr int k_max_peel_count = 16;

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                    SlangCompiler* compiler, PickOverlay* pick_overlay);
  void shutdown();

  void requestPick(float window_x, float window_y, uint16_t modifier_flags,
                   PickRequestKind kind, EditorCamera& camera,
                   SceneInstance& scene, RenderSystem& render_system);

  PickFetchStatus tryFetch(PickResult& out);

  /// Poll fence, advance broad/narrow passes, deliver completed picks to viewport system.
  void poll(EditorCamera& camera, SceneInstance& scene, RenderSystem& render_system,
            ViewportPickSystem& viewport_pick);

 private:
  enum class ActivePickPass : uint8_t {
    none = 0,
    broad = 1,
    narrow = 2,
  };

  struct GpuPickPass {
    VkFence fence{VK_NULL_HANDLE};
    VkCommandBuffer command_buffer{VK_NULL_HANDLE};
    bool active{false};
  };

  bool beginPickRequest(EditorCamera& camera, SceneInstance& scene,
                        RenderSystem& render_system);
  void submitBroadPass(const Ray& ray, VkBuffer instance_buffer,
                       uint32_t instance_count);
  void submitNarrowPass();
  void finishPickRequest();
  void resetGpuPass();
  void cancelInFlight();

  VulkanContext* m_context{nullptr};
  PickOverlay* m_pick_overlay{nullptr};
  RenderSystem* m_render_system{nullptr};
  PickBroadPhase m_broad_phase;

  bool m_request_active{false};
  float m_window_x{0.0f};
  float m_window_y{0.0f};
  uint16_t m_modifier_flags{0};
  PickRequestKind m_kind{PickRequestKind::single_click};
  ActivePickPass m_active_pass{ActivePickPass::none};

  int32_t m_pixel_x{0};
  int32_t m_pixel_y{0};
  glm::mat4 m_view{1.0f};
  glm::mat4 m_projection{1.0f};
  eastl::vector<PickOverlay::PickDraw> m_cached_draws;
  eastl::vector<PickOverlay::PickDraw> m_narrow_draws;
  eastl::vector<BroadHit> m_sorted_broad_hits;
  eastl::vector<EntityId> m_broad_promoted_hits;

  GpuPickPass m_gpu_pass{};
  PickResult m_ready_result{};
  bool m_result_ready{false};
};

}  // namespace Blunder
