#include "runtime/function/editor/viewport_pick_system.h"

#include "runtime/function/editor/viewport_pick_delivery.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <glm/glm.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/event/key_event.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/overlay/overlay_system.h"
#include "runtime/function/render/pick/pick_entity_promotion.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/function/ui/ui_host.h"
#include "runtime/function/ui/view_models/editor_panels_view_model.h"

namespace Blunder {

namespace {

bool isSameClickPixel(const glm::vec2& a, const glm::vec2& b) {
  return glm::length(a - b) <= ViewportPickSystem::k_click_cycle_pixel_tolerance;
}

size_t indexInPeelList(const eastl::vector<EntityId>& hits, EntityId entity_id) {
  for (size_t i = 0; i < hits.size(); ++i) {
    if (hits[i] == entity_id) {
      return i;
    }
  }
  return hits.size();
}

bool pickDebugEnabled() {
  const char* env = std::getenv("BLUNDER_PICK_DEBUG");
  return env != nullptr && env[0] == '1';
}

}  // namespace

void ViewportPickSystem::applySelection(const EntityId entity_id,
                                        const uint16_t modifier_flags) const {
  EditorSelectionSystem* selection = g_runtime_global_context.m_editor_selection.get();
  if (selection == nullptr) {
    return;
  }

  if (!isValid(entity_id)) {
    selection->clearSelection();
    notifyEditorUi();
    return;
  }

  if (keyModifiersCtrl(modifier_flags)) {
    selection->toggleSelection(entity_id);
  } else if (keyModifiersShift(modifier_flags)) {
    selection->addToSelection(entity_id);
  } else {
    selection->setSelection(entity_id);
  }
  notifyEditorUi();
}

void ViewportPickSystem::notifyEditorUi() const {
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->syncInspectorFromSelection();
    // Viewport pick is SDL-driven; unlike Slint hierarchy clicks it does not
    // invalidate the window. Force a Skia composite so outline/gizmo appear without
    // waiting for camera movement (zero-copy idle pacing otherwise skips composite).
    g_runtime_global_context.m_slint_system->markFullSkiaRefresh();
  }
  if (g_runtime_global_context.m_ui_host) {
    g_runtime_global_context.m_ui_host->panels().markDirty(
        EditorPanelDirty::inspector);
    g_runtime_global_context.m_ui_host->panels().markDirty(
        EditorPanelDirty::hierarchy);
  }
}

void ViewportPickSystem::requestPick(const float window_x, const float window_y,
                                     const uint16_t modifier_flags,
                                     const PickRequestKind kind) {
  RenderSystem* render_system = g_runtime_global_context.m_render_system.get();
  if (render_system == nullptr || render_system->getOverlaySystem() == nullptr) {
    return;
  }
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system
          ? g_runtime_global_context.m_scene_system->getActiveInstance()
          : nullptr;
  EditorCamera* camera = render_system->getEditorCamera();
  if (scene == nullptr || camera == nullptr) {
    return;
  }

  if (pickDebugEnabled()) {
    LOG_INFO("[ViewportPick] requestPick kind={} at ({}, {})", static_cast<int>(kind),
             window_x, window_y);
  }

  render_system->getOverlaySystem()->hybridPick().requestPick(
      window_x, window_y, modifier_flags, kind, *camera, *scene, *render_system);
  render_system->requestViewportRedraw();
}

EntityId ViewportPickSystem::pickWithCyclingFromResult(const PickResult& result,
                                                       SceneInstance& scene) {
  const glm::vec2 click_pixel(result.window_x, result.window_y);
  const bool repeat_click = isSameClickPixel(click_pixel, m_last_click_pixel);
  const bool no_modifiers = !keyModifiersCtrl(result.modifier_flags) &&
                            !keyModifiersShift(result.modifier_flags);

  eastl::vector<EntityId> promoted_hits = result.peel_hits;

  if (repeat_click && no_modifiers) {
    if (promoted_hits.size() <= 1 && m_last_peel_hits.size() > 1) {
      promoted_hits = m_last_peel_hits;
    }
    if (promoted_hits.size() <= 1) {
      m_last_click_pixel = click_pixel;
      m_last_peel_hits = promoted_hits;
      m_cycle_index = 0;
      return promoted_hits.empty() ? k_invalid_entity_id : promoted_hits.front();
    }

    EditorSelectionSystem* selection = g_runtime_global_context.m_editor_selection.get();
    const EntityId current =
        selection ? selection->getPrimarySelection() : k_invalid_entity_id;
    const size_t current_index = indexInPeelList(promoted_hits, current);
    if (current_index < promoted_hits.size()) {
      m_cycle_index = (current_index + 1) % promoted_hits.size();
    } else {
      m_cycle_index = (m_cycle_index + 1) % promoted_hits.size();
    }
    m_last_click_pixel = click_pixel;
    m_last_peel_hits = promoted_hits;
    return promoted_hits[m_cycle_index];
  }

  m_last_click_pixel = click_pixel;
  m_last_peel_hits = promoted_hits;
  m_cycle_index = 0;
  return promoted_hits.empty() ? k_invalid_entity_id : promoted_hits.front();
}

void ViewportPickSystem::onViewportLeftPressed(const float window_x,
                                               const float window_y) {
  m_left_pressed_in_viewport = true;
  m_left_press_origin = glm::vec2(window_x, window_y);
  m_left_drag_exceeded = false;
  m_camera_interaction_since_left_press = false;
  if (pickDebugEnabled()) {
    LOG_INFO("[ViewportPick] left pressed at ({}, {})", window_x, window_y);
  }
}

void ViewportPickSystem::onViewportLeftReleased(
    const float window_x, const float window_y, const uint16_t modifier_flags) {
  const bool had_viewport_press = m_left_pressed_in_viewport;
  if (had_viewport_press) {
    m_left_pressed_in_viewport = false;
  }

  if (pickDebugEnabled()) {
    LOG_INFO("[ViewportPick] left released at ({}, {}) had_press={}", window_x,
             window_y, had_viewport_press);
  }

  if (m_suppress_next_left_release_pick) {
    m_suppress_next_left_release_pick = false;
    return;
  }

  RenderSystem* render_system = g_runtime_global_context.m_render_system.get();
  if (render_system == nullptr) {
    return;
  }
  EditorCamera* camera = render_system->getEditorCamera();
  if (camera == nullptr ||
      !camera->isWindowPositionInViewport(glm::vec2(window_x, window_y))) {
    return;
  }

  if (had_viewport_press) {
    if (m_left_drag_exceeded || m_camera_interaction_since_left_press ||
        camera->isViewportInteracting()) {
      return;
    }
  } else if (camera->isViewportInteracting()) {
    if (pickDebugEnabled()) {
      LOG_INFO("[ViewportPick] release without prior press skipped (camera interacting)");
    }
    return;
  }

  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->hidePiercingMenu();
  }

  const glm::vec2 click_pixel(window_x, window_y);
  const bool repeat_click = isSameClickPixel(click_pixel, m_last_click_pixel);
  const bool no_modifiers =
      !keyModifiersCtrl(modifier_flags) && !keyModifiersShift(modifier_flags);

  if (repeat_click && no_modifiers && m_last_peel_hits.size() > 1) {
    EditorSelectionSystem* selection = g_runtime_global_context.m_editor_selection.get();
    const EntityId current =
        selection ? selection->getPrimarySelection() : k_invalid_entity_id;
    const size_t current_index = indexInPeelList(m_last_peel_hits, current);
    if (current_index < m_last_peel_hits.size()) {
      m_cycle_index = (current_index + 1) % m_last_peel_hits.size();
    } else {
      m_cycle_index = (m_cycle_index + 1) % m_last_peel_hits.size();
    }
    m_last_click_pixel = click_pixel;
    applySelection(m_last_peel_hits[m_cycle_index], modifier_flags);
    return;
  }

  m_last_click_pixel = click_pixel;
  m_cycle_index = 0;

  SceneInstance* scene =
      g_runtime_global_context.m_scene_system
          ? g_runtime_global_context.m_scene_system->getActiveInstance()
          : nullptr;
  if (scene == nullptr) {
    return;
  }

  const EntityId sync_leaf =
      render_system->pickEntityAtWindowPosition(window_x, window_y);
  const EntityId sync_front = promotePickEntity(*scene, sync_leaf);
  if (isValid(sync_front)) {
    applySelection(sync_front, modifier_flags);
  }

  const eastl::vector<EntityId> sync_leaves =
      render_system->pickAllEntitiesAtWindowPosition(window_x, window_y);
  m_last_peel_hits = dedupePromotedPickHits(*scene, sync_leaves);

  if (pickDebugEnabled()) {
    LOG_INFO("[ViewportPick] sync pick leaf={} front={} peel_list={}", sync_leaf,
             sync_front, m_last_peel_hits.size());
  }

  m_deferred_multi_peel = true;
  m_deferred_multi_peel_wait_one_tick = true;
  m_deferred_window_x = window_x;
  m_deferred_window_y = window_y;
  m_deferred_modifier_flags = modifier_flags;
}

void ViewportPickSystem::tickDeferredPickRequest() {
  if (advanceDeferredMultiPeelTick(m_deferred_multi_peel,
                                   m_deferred_multi_peel_wait_one_tick) !=
      DeferredPickTickAction::flush) {
    return;
  }

  requestPick(m_deferred_window_x, m_deferred_window_y, m_deferred_modifier_flags,
              PickRequestKind::multi_peel);
}

void ViewportPickSystem::onViewportPointerMoved(const float window_x,
                                                const float window_y) {
  if (!m_left_pressed_in_viewport || m_left_drag_exceeded) {
    return;
  }
  const glm::vec2 current(window_x, window_y);
  if (glm::length(current - m_left_press_origin) > k_click_drag_threshold_px) {
    m_left_drag_exceeded = true;
  }
}

void ViewportPickSystem::onCameraInteractionStarted() {
  if (m_left_pressed_in_viewport) {
    m_camera_interaction_since_left_press = true;
  }
}

void ViewportPickSystem::suppressNextLeftReleasePick() {
  m_suppress_next_left_release_pick = true;
}

void ViewportPickSystem::onPiercingMenuRequest(const float window_x,
                                               const float window_y,
                                               const uint16_t modifier_flags) {
  RenderSystem* render_system = g_runtime_global_context.m_render_system.get();
  SceneInstance* scene =
      g_runtime_global_context.m_scene_system
          ? g_runtime_global_context.m_scene_system->getActiveInstance()
          : nullptr;
  if (render_system == nullptr || scene == nullptr) {
    return;
  }

  EditorCamera* camera = render_system->getEditorCamera();
  if (camera == nullptr ||
      !camera->isWindowPositionInViewport(glm::vec2(window_x, window_y))) {
    return;
  }

  requestPick(window_x, window_y, modifier_flags, PickRequestKind::piercing_menu);
}

void ViewportPickSystem::deliverPickResult(const PickResult& result,
                                           SceneInstance& scene) {
  if (pickDebugEnabled()) {
    LOG_INFO("[ViewportPick] deliverPickResult kind={} peel_hits={}",
             static_cast<int>(result.kind), result.peel_hits.size());
  }

  if (result.kind == PickRequestKind::piercing_menu) {
    SlintSystem* slint = g_runtime_global_context.m_slint_system.get();
    if (slint == nullptr) {
      return;
    }

    const eastl::vector<EntityId> promoted_hits = result.peel_hits;
    if (promoted_hits.empty()) {
      slint->hidePiercingMenu();
      return;
    }

    eastl::vector<SlintSystem::PiercingMenuItem> items;
    items.reserve(promoted_hits.size());
    for (const EntityId entity_id : promoted_hits) {
      const Entity* entity = scene.getEntity(entity_id);
      SlintSystem::PiercingMenuItem item{};
      item.entity_id = entity_id;
      item.name = entity != nullptr ? entity->getName() : eastl::string("Entity");
      items.push_back(eastl::move(item));
    }

    m_last_click_pixel = glm::vec2(result.window_x, result.window_y);
    m_last_peel_hits = promoted_hits;
    m_cycle_index = 0;

    const bool add_mode = keyModifiersShift(result.modifier_flags);
    slint->showPiercingMenu(items, result.window_x, result.window_y, add_mode);
    return;
  }

  if (result.kind == PickRequestKind::multi_peel) {
    const glm::vec2 click_pixel(result.window_x, result.window_y);
    const eastl::vector<EntityId> promoted_hits = result.peel_hits;

    m_last_click_pixel = click_pixel;
    m_last_peel_hits = promoted_hits;
    m_cycle_index = 0;

    EditorSelectionSystem* selection = g_runtime_global_context.m_editor_selection.get();
    const EntityId current =
        selection ? selection->getPrimarySelection() : k_invalid_entity_id;
    const MultiPeelDeliveryAction action =
        resolveMultiPeelDelivery(current, promoted_hits);
    if (pickDebugEnabled()) {
      LOG_INFO("[ViewportPick] multi_peel delivery current={} apply={} entity={}",
               current, action.apply_selection, action.selection_entity);
    }
    if (action.apply_selection) {
      applySelection(action.selection_entity, result.modifier_flags);
    }

    if (g_runtime_global_context.m_render_system) {
      g_runtime_global_context.m_render_system->requestViewportRedraw();
    }
    return;
  }

  const EntityId picked = pickWithCyclingFromResult(result, scene);
  applySelection(picked, result.modifier_flags);
}

}  // namespace Blunder
