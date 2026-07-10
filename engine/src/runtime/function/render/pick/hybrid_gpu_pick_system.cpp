#include "runtime/function/render/pick/hybrid_gpu_pick_system.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/editor/viewport_pick_system.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/overlay/overlay_system.h"
#include "runtime/function/render/overlay/pick_overlay.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/slang/slang_compiler.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

void HybridGpuPickSystem::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                                     SlangCompiler* compiler,
                                     PickOverlay* pick_overlay) {
  m_context = ctx;
  m_pick_overlay = pick_overlay;
  m_broad_phase.initialize(ctx, alloc, compiler);

  if (m_context == nullptr) {
    return;
  }

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  const VkResult fence_result =
      vkCreateFence(m_context->getDevice(), &fence_info, nullptr,
                    &m_gpu_pass.fence);
  if (fence_result != VK_SUCCESS) {
    LOG_FATAL("[HybridGpuPickSystem] vkCreateFence failed: {}",
              static_cast<int>(fence_result));
  }
}

void HybridGpuPickSystem::shutdown() {
  cancelInFlight();
  m_broad_phase.shutdown();
  if (m_context != nullptr && m_gpu_pass.fence != VK_NULL_HANDLE) {
    vkDestroyFence(m_context->getDevice(), m_gpu_pass.fence, nullptr);
    m_gpu_pass.fence = VK_NULL_HANDLE;
  }
  m_pick_overlay = nullptr;
  m_context = nullptr;
  m_render_system = nullptr;
}

void HybridGpuPickSystem::resetGpuPass() {
  if (m_context == nullptr) {
    return;
  }
  if (m_gpu_pass.command_buffer != VK_NULL_HANDLE) {
    m_context->freeImmediateCommandBuffer(m_gpu_pass.command_buffer);
    m_gpu_pass.command_buffer = VK_NULL_HANDLE;
  }
  if (m_gpu_pass.fence != VK_NULL_HANDLE) {
    vkResetFences(m_context->getDevice(), 1, &m_gpu_pass.fence);
  }
  m_gpu_pass.active = false;
}

void HybridGpuPickSystem::cancelInFlight() {
  if (m_context != nullptr && m_gpu_pass.fence != VK_NULL_HANDLE &&
      m_gpu_pass.active) {
    vkWaitForFences(m_context->getDevice(), 1, &m_gpu_pass.fence, VK_TRUE,
                    UINT64_MAX);
  }
  resetGpuPass();
  m_request_active = false;
  m_result_ready = false;
  m_active_pass = ActivePickPass::none;
  m_cached_draws.clear();
  m_narrow_draws.clear();
  m_sorted_broad_hits.clear();
  m_broad_promoted_hits.clear();
  m_render_system = nullptr;
}

void HybridGpuPickSystem::requestPick(const float window_x, const float window_y,
                                      const uint16_t modifier_flags,
                                      const PickRequestKind kind,
                                      EditorCamera& camera, SceneInstance& scene,
                                      RenderSystem& render_system) {
  if (m_pick_overlay == nullptr || m_context == nullptr) {
    return;
  }

  cancelInFlight();

  m_window_x = window_x;
  m_window_y = window_y;
  m_modifier_flags = modifier_flags;
  m_kind = kind;
  m_request_active = true;
  m_result_ready = false;
  m_render_system = &render_system;

  if (!beginPickRequest(camera, scene, render_system)) {
    m_request_active = false;
    m_render_system = nullptr;
    return;
  }

  OverlaySystem* overlay = render_system.getOverlaySystem();
  const VkBuffer instance_buffer = overlay->pickInstances().gpuBuffer();
  const uint32_t instance_count = overlay->pickInstances().gpuInstanceCount();
  if (instance_buffer == VK_NULL_HANDLE || instance_count == 0) {
    m_request_active = false;
    m_render_system = nullptr;
    return;
  }

  const Ray ray = camera.makeRayFromWindowPosition(glm::vec2(window_x, window_y));
  submitBroadPass(ray, instance_buffer, instance_count);
}

bool HybridGpuPickSystem::beginPickRequest(EditorCamera& camera, SceneInstance& scene,
                                           RenderSystem& render_system) {
  (void)scene;
  if (!m_pick_overlay->resolvePickPixel(m_window_x, m_window_y, camera,
                                        m_pixel_x, m_pixel_y)) {
    return false;
  }

  OverlaySystem* overlay = render_system.getOverlaySystem();
  if (overlay == nullptr) {
    return false;
  }

  m_cached_draws = overlay->pickInstances().pickDraws();
  if (m_cached_draws.empty()) {
    return false;
  }

  m_view = camera.getViewMatrix();
  m_projection = camera.getProjectionMatrix();
  return true;
}

void HybridGpuPickSystem::submitBroadPass(const Ray& ray, VkBuffer instance_buffer,
                                          const uint32_t instance_count) {
  if (m_context == nullptr || m_render_system == nullptr) {
    return;
  }

  resetGpuPass();
  m_active_pass = ActivePickPass::broad;

  m_gpu_pass.command_buffer = m_context->beginImmediateCommands();
  m_broad_phase.recordDispatch(m_gpu_pass.command_buffer, ray, instance_buffer,
                               instance_count);
  m_context->submitImmediateCommandsNoWait(m_gpu_pass.command_buffer,
                                           m_gpu_pass.fence);
  m_gpu_pass.active = true;
}

void HybridGpuPickSystem::submitNarrowPass() {
  if (m_pick_overlay == nullptr || m_context == nullptr ||
      m_render_system == nullptr || m_narrow_draws.empty()) {
    return;
  }

  resetGpuPass();
  m_active_pass = ActivePickPass::narrow;

  m_gpu_pass.command_buffer = m_context->beginImmediateCommands();
  m_pick_overlay->recordPixelPickPass(m_gpu_pass.command_buffer, m_pixel_x,
                                      m_pixel_y, m_narrow_draws, m_view,
                                      m_projection, *m_render_system);
  m_context->submitImmediateCommandsNoWait(m_gpu_pass.command_buffer,
                                           m_gpu_pass.fence);
  m_gpu_pass.active = true;
}

void HybridGpuPickSystem::finishPickRequest() {
  m_ready_result.kind = m_kind;
  m_ready_result.window_x = m_window_x;
  m_ready_result.window_y = m_window_y;
  m_ready_result.modifier_flags = m_modifier_flags;
  m_ready_result.peel_hits = m_broad_promoted_hits;
  m_ready_result.entity_id =
      m_broad_promoted_hits.empty() ? k_invalid_entity_id
                                    : m_broad_promoted_hits.front();

  m_result_ready = true;
  m_request_active = false;
  m_active_pass = ActivePickPass::none;
  m_render_system = nullptr;
  resetGpuPass();
}

PickFetchStatus HybridGpuPickSystem::tryFetch(PickResult& out) {
  if (m_result_ready) {
    out = m_ready_result;
    m_result_ready = false;
    return PickFetchStatus::ready;
  }
  if (m_request_active || m_gpu_pass.active) {
    return PickFetchStatus::pending;
  }
  return PickFetchStatus::idle;
}

void HybridGpuPickSystem::poll(EditorCamera& /*camera*/, SceneInstance& scene,
                               RenderSystem& /*render_system*/,
                               ViewportPickSystem& viewport_pick) {
  if (m_context == nullptr || m_pick_overlay == nullptr) {
    return;
  }

  if (m_gpu_pass.active && m_gpu_pass.fence != VK_NULL_HANDLE) {
    const VkResult fence_status =
        vkGetFenceStatus(m_context->getDevice(), m_gpu_pass.fence);
    if (fence_status == VK_NOT_READY) {
      return;
    }
    if (fence_status != VK_SUCCESS) {
      cancelInFlight();
      return;
    }

    m_gpu_pass.active = false;

    if (m_active_pass == ActivePickPass::broad) {
      eastl::vector<BroadHit> broad_hits;
      m_broad_phase.readbackHits(broad_hits);
      m_sorted_broad_hits = sortBroadHitsByDistance(eastl::move(broad_hits));
      m_broad_promoted_hits =
          promoteAndDedupeBroadHits(scene, m_sorted_broad_hits);

      if (m_kind == PickRequestKind::piercing_menu) {
        finishPickRequest();
        PickResult result{};
        if (tryFetch(result) == PickFetchStatus::ready) {
          viewport_pick.deliverPickResult(result, scene);
        }
        return;
      }

      if (m_broad_promoted_hits.empty()) {
        finishPickRequest();
        PickResult result{};
        if (tryFetch(result) == PickFetchStatus::ready) {
          viewport_pick.deliverPickResult(result, scene);
        }
        return;
      }

      eastl::vector<EntityId> broad_leaf_ids;
      broad_leaf_ids.reserve(m_sorted_broad_hits.size());
      for (const BroadHit& hit : m_sorted_broad_hits) {
        broad_leaf_ids.push_back(hit.entity_id);
      }
      m_narrow_draws =
          PickOverlay::filterPickDrawsToEntities(m_cached_draws, broad_leaf_ids);
      if (m_narrow_draws.empty()) {
        finishPickRequest();
        PickResult result{};
        if (tryFetch(result) == PickFetchStatus::ready) {
          viewport_pick.deliverPickResult(result, scene);
        }
        return;
      }

      submitNarrowPass();
      return;
    }

    if (m_active_pass == ActivePickPass::narrow) {
      const PickOverlay::PickSample sample =
          m_pick_overlay->readSubmittedPickSample();
      if (!PickOverlay::isPickSampleMiss(sample.entity_id, sample.depth) &&
          !m_broad_promoted_hits.empty()) {
        (void)sample;
      }

      finishPickRequest();
      PickResult result{};
      if (tryFetch(result) == PickFetchStatus::ready) {
        viewport_pick.deliverPickResult(result, scene);
      }
      return;
    }

    resetGpuPass();
    return;
  }

  if (m_result_ready) {
    PickResult result{};
    if (tryFetch(result) == PickFetchStatus::ready) {
      viewport_pick.deliverPickResult(result, scene);
    }
  }
}

}  // namespace Blunder
