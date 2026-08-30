#include "runtime/function/slint/slint_system.h"

#include <algorithm>
#include <cmath>
#include <exception>

#include <SDL3/SDL.h>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/object/object.h"
#include "runtime/function/editor/document_history_helpers.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/hierarchy_row_icons.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/editor/inspector_add_ops.h"
#include "runtime/function/editor/inspector_behaviour_ops.h"
#include "runtime/function/editor/inspector_skeleton_modifier_ops.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/script/behaviour_type_catalog.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/function/ui/ui_host.h"
#include "runtime/function/ui/ui_events.h"
#include "runtime/function/editor/animation_preview_controller.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/transform_edit_viewport_notify.h"
#include "runtime/resource/asset_manager/asset_manager.h"

#include <glm/common.hpp>

namespace Blunder {

void notifyAnimationPreviewAfterSkeletonModifierEdit(
    class RenderSystem* render_system);

namespace {

class ScopedDispatchGuard final {
 public:
  explicit ScopedDispatchGuard(int& depth) : m_depth(depth) { ++m_depth; }
  ~ScopedDispatchGuard() { --m_depth; }

 private:
  int& m_depth;
};

const char* previewTitleSuffix(int kind) {
  switch (kind) {
    case 0:
      return "Transform";
    case 1:
      return "MeshRenderer";
    case 2:
      return "Camera";
    case 3:
      return "Light";
    case 4:
      return "Skeleton";
    case 5:
      return "AnimationTree";
    case 6:
      return "Behaviour";
    case 7:
      return "SkeletonModifier";
    default:
      return "Attachment";
  }
}

const char* uniqueKindNameFromIcon(int kind) {
  switch (kind) {
    case 2:
      return "Camera";
    case 3:
      return "Light";
    case 4:
      return "Skeleton";
    case 5:
      return "AnimationTree";
    default:
      return "";
  }
}

void clearOtherMainCamerasLocal(SceneInstance& scene, EntityId keep_id) {
  scene.forEachCamera([&](EntityId entity_id, const CameraComponent& camera) {
    if (entity_id == keep_id || !camera.is_main) {
      return;
    }
    CameraComponent updated = camera;
    updated.is_main = false;
    scene.setCamera(entity_id, eastl::move(updated));
  });
}

void sanitizeLightLocal(LightComponent& light) {
  light.intensity = std::max(light.intensity, 0.0f);
  light.range = std::max(light.range, 1e-3f);
  light.width = std::max(light.width, 1e-3f);
  light.height = std::max(light.height, 1e-3f);
  light.outer_cone_degrees = glm::clamp(light.outer_cone_degrees, 0.001f, 90.0f);
  light.inner_cone_degrees =
      glm::clamp(light.inner_cone_degrees, 0.0f, light.outer_cone_degrees - 0.001f);
}

int findPreviewCardIndex(
    const eastl::vector<SlintSystem::AttachmentPreviewCardState>& cards,
    EntityId entity_id, int kind, int index) {
  for (size_t i = 0; i < cards.size(); ++i) {
    const auto& card = cards[i];
    if (card.entity_id == entity_id && card.kind == kind && card.index == index) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

eastl::string behaviourPropKindFromCatalog(const eastl::vector<BehaviourCatalogType>& catalog,
                                           const eastl::string& type_name,
                                           const eastl::string& key) {
  const BehaviourCatalogType* type = findBehaviourCatalogType(catalog, type_name);
  if (type == nullptr) {
    return {};
  }
  for (const BehaviourCatalogMember& member : type->members) {
    if (member.name == key) {
      return behaviourCatalogKindName(member.kind);
    }
  }
  return {};
}

}  // namespace

void SlintSystem::closeAttachmentPreviewCards() {
  m_preview_cards.clear();
  g_runtime_global_context.setAttachmentPreviewHasInputFocus(false);
  syncAttachmentPreviewCards();
}

void SlintSystem::pruneUnpinnedPreviewCards(EntityId selected) {
  auto it = std::remove_if(
      m_preview_cards.begin(), m_preview_cards.end(),
      [selected](const AttachmentPreviewCardState& card) {
        return !card.pinned && card.entity_id != selected;
      });
  if (it == m_preview_cards.end()) {
    return;
  }
  m_preview_cards.erase(it, m_preview_cards.end());
  syncAttachmentPreviewCards();
}

void SlintSystem::pruneGonePreviewCards() {
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    if (!m_preview_cards.empty()) {
      m_preview_cards.clear();
      syncAttachmentPreviewCards();
    }
    return;
  }
  auto it = std::remove_if(
      m_preview_cards.begin(), m_preview_cards.end(),
      [scene](const AttachmentPreviewCardState& card) {
        return !hierarchyRowIconAttachmentPresent(
            *scene, card.entity_id,
            static_cast<HierarchyRowIconKind>(card.kind), card.index);
      });
  if (it == m_preview_cards.end()) {
    return;
  }
  m_preview_cards.erase(it, m_preview_cards.end());
  syncAttachmentPreviewCards();
}

void SlintSystem::rebuildHierarchyAfterAttachmentChange() {
  const auto services = lockServices();
  if (services && services->hierarchy && services->scene) {
    if (SceneInstance* scene = services->scene->getActiveInstance()) {
      services->hierarchy->rebuildVisibleTree(scene);
      services->hierarchy->markDirty();
    }
  }
  pruneGonePreviewCards();
  syncHierarchy();
  syncAttachmentPreviewCards();
}

void SlintSystem::handleHierarchyIconPressed(int entity_id, float mouse_x,
                                             float row_width) {
  if ((SDL_GetModState() & SDL_KMOD_ALT) == 0) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  if (!isValid(id)) {
    return;
  }

  const auto services = lockServices();
  if (!services || !services->hierarchy) {
    return;
  }

  eastl::vector<HierarchyRowIconSlot> icons;
  bool found_row = false;
  for (const EditorHierarchyTreeRow& row : services->hierarchy->treeRows()) {
    if (static_cast<EntityId>(row.entity_id) == id) {
      icons = row.icons;
      found_row = true;
      break;
    }
  }
  if (!found_row) {
    SceneInstance* scene =
        services->scene ? services->scene->getActiveInstance() : nullptr;
    if (scene == nullptr) {
      return;
    }
    fillHierarchyRowIcons(*scene, id, icons);
  }

  const int slot_index =
      hitTestHierarchyRowIconIndex(mouse_x, row_width, icons.size());
  if (slot_index < 0 || static_cast<size_t>(slot_index) >= icons.size()) {
    return;
  }
  const HierarchyRowIconSlot& slot = icons[static_cast<size_t>(slot_index)];
  const int kind = static_cast<int>(slot.kind);
  const int index = slot.index;

  const int existing = findPreviewCardIndex(m_preview_cards, id, kind, index);
  if (existing >= 0) {
    if (m_preview_cards[static_cast<size_t>(existing)].pinned) {
      AttachmentPreviewCardState raised =
          m_preview_cards[static_cast<size_t>(existing)];
      m_preview_cards.erase(m_preview_cards.begin() + existing);
      m_preview_cards.push_back(eastl::move(raised));
      syncAttachmentPreviewCards();
      return;
    }
    m_preview_cards.erase(m_preview_cards.begin() + existing);
    syncAttachmentPreviewCards();
    return;
  }

  auto unpinned = std::remove_if(
      m_preview_cards.begin(), m_preview_cards.end(),
      [](const AttachmentPreviewCardState& card) { return !card.pinned; });
  m_preview_cards.erase(unpinned, m_preview_cards.end());

  AttachmentPreviewCardState card{};
  card.entity_id = id;
  card.kind = kind;
  card.index = index;
  card.pinned = false;
  card.x = 72.0f + static_cast<float>(m_preview_cards.size()) * 24.0f;
  card.y = 96.0f + static_cast<float>(m_preview_cards.size()) * 18.0f;
  m_preview_cards.push_back(card);
  syncAttachmentPreviewCards();
}

void SlintSystem::handlePreviewPinToggled(int entity_id, int kind, int index) {
  const int existing = findPreviewCardIndex(
      m_preview_cards, static_cast<EntityId>(entity_id), kind, index);
  if (existing < 0) {
    return;
  }
  m_preview_cards[static_cast<size_t>(existing)].pinned =
      !m_preview_cards[static_cast<size_t>(existing)].pinned;
  syncAttachmentPreviewCards();
}

void SlintSystem::handlePreviewClose(int entity_id, int kind, int index) {
  const int existing = findPreviewCardIndex(
      m_preview_cards, static_cast<EntityId>(entity_id), kind, index);
  if (existing < 0) {
    return;
  }
  m_preview_cards.erase(m_preview_cards.begin() + existing);
  if (m_preview_cards.empty()) {
    g_runtime_global_context.setAttachmentPreviewHasInputFocus(false);
  }
  syncAttachmentPreviewCards();
}

void SlintSystem::syncAttachmentPreviewCards() {
  if (!m_window_component) {
    return;
  }
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;

  try {
    ScopedDispatchGuard guard(m_slint_dispatch_depth);
    m_applying_preview_sync = true;
    auto model = std::make_shared<slint::VectorModel<AttachmentPreviewCardRow>>();
    if (scene != nullptr) {
      eastl::vector<BehaviourCatalogType> catalog;
      if (g_runtime_global_context.m_file_system) {
        const auto catalog_path =
            g_runtime_global_context.m_file_system->getProjectRoot() / ".blunder" /
            "behaviour_catalog.json";
        eastl::string error;
        loadBehaviourTypeCatalog(catalog_path, catalog, error);
      }

      for (const AttachmentPreviewCardState& state : m_preview_cards) {
        if (!hierarchyRowIconAttachmentPresent(
                *scene, state.entity_id,
                static_cast<HierarchyRowIconKind>(state.kind), state.index)) {
          continue;
        }
        AttachmentPreviewCardRow row{};
        row.entity_id = static_cast<int>(state.entity_id);
        row.kind = state.kind;
        row.index = state.index;
        row.pinned = state.pinned;
        row.card_x = state.x;
        row.card_y = state.y;

        eastl::string title;
        if (const Entity* entity = scene->getEntity(state.entity_id)) {
          title = entity->getName();
          const Vec3 euler =
              SceneSerializer::rotationToEulerDegrees(entity->getRotation());
          row.pos_x = entity->getPosition().x;
          row.pos_y = entity->getPosition().y;
          row.pos_z = entity->getPosition().z;
          row.rot_x = euler.x;
          row.rot_y = euler.y;
          row.rot_z = euler.z;
          row.scale_x = entity->getScale().x;
          row.scale_y = entity->getScale().y;
          row.scale_z = entity->getScale().z;
        }
        title += " — ";
        title += previewTitleSuffix(state.kind);
        row.title = slint::SharedString(title.c_str());

        if (const MeshRendererComponent* mesh = scene->getMeshRenderer(state.entity_id)) {
          if (mesh->mesh) {
            row.mesh_path = slint::SharedString(mesh->mesh->getVirtualPath().c_str());
          } else {
            row.mesh_path = slint::SharedString("MeshRenderer");
          }
        }
        if (const CameraComponent* camera = scene->getCamera(state.entity_id)) {
          row.camera_fov = camera->vertical_fov_degrees;
          row.camera_near = camera->near_clip;
          row.camera_far = camera->far_clip;
          row.camera_is_main = camera->is_main;
        }
        if (const LightComponent* light = scene->getLight(state.entity_id)) {
          row.light_type = static_cast<int>(light->type);
          row.light_color_r = light->color.r;
          row.light_color_g = light->color.g;
          row.light_color_b = light->color.b;
          row.light_intensity = light->intensity;
          row.light_enabled = light->enabled;
          row.light_contribution = static_cast<int>(light->contribution);
          row.light_range = light->range;
          row.light_inner_cone = light->inner_cone_degrees;
          row.light_outer_cone = light->outer_cone_degrees;
          row.light_width = light->width;
          row.light_height = light->height;
        }

        Object* object = scene->findBoundObject(state.entity_id);
        if (object != nullptr && object->hasAnimationTree()) {
          row.tree_canvas_enabled = true;
        }
        if (object != nullptr &&
            state.kind == static_cast<int>(HierarchyRowIconKind::Behaviour) &&
            state.index >= 0 &&
            static_cast<size_t>(state.index) < object->getBehaviourCount()) {
          eastl::vector<InspectorBehaviourRowData> rows;
          buildInspectorBehaviourRows(object, catalog, rows);
          if (static_cast<size_t>(state.index) < rows.size()) {
            const InspectorBehaviourRowData& src = rows[static_cast<size_t>(state.index)];
            row.behaviour_id = static_cast<int>(src.behaviour_id);
            row.behaviour_type = slint::SharedString(src.type_name.c_str());
            row.behaviour_missing = src.missing;
            auto props = std::make_shared<slint::VectorModel<PreviewBehaviourPropRow>>();
            for (const InspectorBehaviourPropRowData& prop : src.props) {
              PreviewBehaviourPropRow slint_prop{};
              slint_prop.key = slint::SharedString(prop.key.c_str());
              slint_prop.kind = slint::SharedString(prop.kind.c_str());
              slint_prop.bool_value = prop.bool_value;
              slint_prop.number_value = prop.number_value;
              slint_prop.string_value = slint::SharedString(prop.string_value.c_str());
              slint_prop.missing_type = prop.missing_type;
              slint_prop.clip_name_invalid = prop.clip_name_invalid;
              props->push_back(slint_prop);
            }
            row.behaviour_props = props;
          }
        }
        if (object != nullptr &&
            state.kind == static_cast<int>(HierarchyRowIconKind::SkeletonModifier) &&
            state.index >= 0 &&
            static_cast<size_t>(state.index) < object->getSkeletonModifierCount()) {
          eastl::vector<InspectorSkeletonModifierRowData> rows;
          buildInspectorSkeletonModifierRows(object, scene, rows);
          if (static_cast<size_t>(state.index) < rows.size()) {
            const InspectorSkeletonModifierRowData& src =
                rows[static_cast<size_t>(state.index)];
            row.modifier_type = slint::SharedString(src.type_name.c_str());
            row.modifier_enabled = src.enabled;
            row.modifier_missing = src.missing;
            row.modifier_bone = slint::SharedString(src.bone_name.c_str());
            row.modifier_open_amount = src.open_amount;
            row.modifier_attach_driven = src.attach_driven;
            row.modifier_target_x = src.target.x;
            row.modifier_target_y = src.target.y;
            row.modifier_target_z = src.target.z;
            row.modifier_child_name =
                slint::SharedString(src.child_entity_name.c_str());
          }
        }
        model->push_back(row);
      }
    }
    m_window_component->operator->()->set_attachment_preview_cards(model);
    m_applying_preview_sync = false;
  } catch (const std::exception& e) {
    m_applying_preview_sync = false;
    LOG_ERROR("[SlintSystem::syncAttachmentPreviewCards] {}", e.what());
  } catch (...) {
    m_applying_preview_sync = false;
    LOG_ERROR("[SlintSystem::syncAttachmentPreviewCards] unknown exception");
  }
}

void SlintSystem::applyPreviewTransform(int entity_id, int kind, int index,
                                        float pos_x, float pos_y, float pos_z,
                                        float rot_x, float rot_y, float rot_z,
                                        float scale_x, float scale_y,
                                        float scale_z) {
  (void)kind;
  (void)index;
  if (!m_window_component || m_applying_preview_sync) {
    return;
  }
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  Entity* entity = scene->getEntity(id);
  if (entity == nullptr) {
    return;
  }
  const Vec3 before_pos = entity->getPosition();
  const Quat before_rot = entity->getRotation();
  const Vec3 before_scale = entity->getScale();
  const Vec3 after_pos(pos_x, pos_y, pos_z);
  const Quat after_rot =
      SceneSerializer::rotationFromEulerDegrees(Vec3(rot_x, rot_y, rot_z));
  const Vec3 after_scale(scale_x, scale_y, scale_z);
  if (before_pos == after_pos && before_rot == after_rot &&
      before_scale == after_scale) {
    return;
  }
  entity->setPosition(after_pos);
  entity->setRotation(after_rot);
  entity->setScale(after_scale);
  pushDocumentCommand(makeSetEntityTransformCommand(
      scene, id, before_pos, before_rot, before_scale, after_pos, after_rot,
      after_scale, currentSelectionSnapshot(), currentSelectionSnapshot()));
  if (services && services->render_system) {
    services->render_system->requestViewportRedraw();
  }
  syncInspectorFromSelection();
}

void SlintSystem::applyPreviewCamera(int entity_id, int kind, int index, float fov,
                                     float near_clip, float far_clip, bool is_main,
                                     bool commit) {
  (void)kind;
  (void)index;
  if (!m_window_component || m_applying_preview_sync) {
    return;
  }
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  const CameraComponent* existing = scene->getCamera(id);
  if (existing == nullptr) {
    return;
  }
  const CameraComponent current = *existing;
  CameraComponent after = current;
  after.vertical_fov_degrees = fov;
  after.near_clip = near_clip;
  after.far_clip = far_clip;
  after.is_main = is_main;
  const bool same = after.vertical_fov_degrees == current.vertical_fov_degrees &&
                    after.near_clip == current.near_clip &&
                    after.far_clip == current.far_clip && after.is_main == current.is_main;
  if (same && !m_inspector_camera_edit_open) {
    return;
  }
  if (!commit) {
    if (!m_inspector_camera_edit_open) {
      m_inspector_camera_edit_before = current;
      m_inspector_camera_edit_open = true;
    }
    if (!same) {
      if (after.is_main) {
        clearOtherMainCamerasLocal(*scene, id);
      }
      scene->setCamera(id, after);
      notifyViewportAfterInspectorTransformEdit(
          services ? services->render_system.get() : nullptr, this);
    }
    return;
  }
  const CameraComponent command_before =
      m_inspector_camera_edit_open ? m_inspector_camera_edit_before : current;
  m_inspector_camera_edit_open = false;
  if (after.vertical_fov_degrees == command_before.vertical_fov_degrees &&
      after.near_clip == command_before.near_clip &&
      after.far_clip == command_before.far_clip && after.is_main == command_before.is_main) {
    return;
  }
  if (after.is_main) {
    clearOtherMainCamerasLocal(*scene, id);
  }
  scene->setCamera(id, after);
  pushDocumentCommand(makeSetCameraComponentCommand(
      scene, id, command_before, after, currentSelectionSnapshot(),
      currentSelectionSnapshot()));
  notifyViewportAfterInspectorTransformEdit(
      services ? services->render_system.get() : nullptr, this);
  syncInspectorCameraFromSelection();
}

void SlintSystem::applyPreviewLight(int entity_id, int kind, int index,
                                    int light_type, float color_r, float color_g,
                                    float color_b, float intensity, bool enabled,
                                    float range, bool commit) {
  (void)kind;
  (void)index;
  if (!m_window_component || m_applying_preview_sync) {
    return;
  }
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  const LightComponent* existing = scene->getLight(id);
  if (existing == nullptr) {
    return;
  }
  const LightComponent current = *existing;
  LightComponent after = current;
  after.type = static_cast<LightType>(light_type);
  after.color = Vec3(color_r, color_g, color_b);
  after.intensity = intensity;
  after.enabled = enabled;
  after.range = range;
  sanitizeLightLocal(after);
  const bool same = after.type == current.type && after.color == current.color &&
                    after.intensity == current.intensity &&
                    after.enabled == current.enabled && after.range == current.range;
  if (same && !m_inspector_light_edit_open) {
    return;
  }
  if (!commit) {
    if (!m_inspector_light_edit_open) {
      m_inspector_light_edit_before = current;
      m_inspector_light_edit_open = true;
    }
    if (!same) {
      scene->setLight(id, after);
      notifyViewportAfterInspectorLightEdit(
          services ? services->render_system.get() : nullptr, this);
    }
    return;
  }
  const LightComponent command_before =
      m_inspector_light_edit_open ? m_inspector_light_edit_before : current;
  m_inspector_light_edit_open = false;
  if (after.type == command_before.type && after.color == command_before.color &&
      after.intensity == command_before.intensity &&
      after.enabled == command_before.enabled && after.range == command_before.range) {
    return;
  }
  scene->setLight(id, after);
  pushDocumentCommand(makeSetLightComponentCommand(
      scene, id, command_before, after, currentSelectionSnapshot(),
      currentSelectionSnapshot()));
  notifyViewportAfterInspectorLightEdit(
      services ? services->render_system.get() : nullptr, this);
  syncInspectorLightFromSelection();
}

void SlintSystem::applyPreviewUniqueRemove(int entity_id, int kind, int index) {
  (void)index;
  const char* name = uniqueKindNameFromIcon(kind);
  InspectorUniqueKind unique{};
  if (name[0] == '\0' || !parseInspectorUniqueKind(eastl::string(name), unique)) {
    return;
  }
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  AssetManager* assets = g_runtime_global_context.m_asset_manager.get();
  InspectorUniqueRemoveSnapshot snapshot;
  if (!applyInspectorUniqueRemove(assets, *scene, id, unique, snapshot)) {
    return;
  }
  pushDocumentCommand(makeRemoveUniqueAttachmentCommand(
      scene, assets, id, unique, eastl::move(snapshot), currentSelectionSnapshot(),
      currentSelectionSnapshot()));
  syncInspectorFromSelection();
  rebuildHierarchyAfterAttachmentChange();
}

void SlintSystem::applyPreviewTreeCanvas(int entity_id) {
  const auto services = lockServices();
  if (!services || !services->selection) {
    return;
  }
  services->selection->setSelection(static_cast<EntityId>(entity_id));
  if (const auto host = m_ui_host.lock()) {
    if (g_runtime_global_context.m_animation_preview &&
        !g_runtime_global_context.m_animation_preview->assetGuid().empty()) {
      host->enqueue(UiEvent::withPath(
          UiEventKind::openAnimationTreeCanvasFromGuid,
          g_runtime_global_context.m_animation_preview->assetGuid()));
    }
  }
}

void SlintSystem::applyPreviewBehaviourRemove(int entity_id, int kind, int index) {
  (void)kind;
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  Object* object = scene->findBoundObject(id);
  if (object == nullptr || index < 0 ||
      static_cast<size_t>(index) >= object->getBehaviourCount()) {
    return;
  }
  const BehaviourId behaviour_id = object->getBehaviourIdAt(static_cast<size_t>(index));
  const char* type_name = object->getBehaviourTypeName(behaviour_id);
  if (type_name == nullptr) {
    return;
  }
  eastl::vector<SceneBehaviourProperty> properties;
  if (const eastl::vector<SceneBehaviourProperty>* bag =
          object->getBehaviourProperties(behaviour_id);
      bag != nullptr) {
    properties = *bag;
  }
  if (!object->removeBehaviour(behaviour_id)) {
    return;
  }
  pushDocumentCommand(makeRemoveBehaviourCommand(
      scene, id, behaviour_id, static_cast<size_t>(index), type_name,
      eastl::move(properties), currentSelectionSnapshot(),
      currentSelectionSnapshot()));
  syncInspectorBehavioursFromSelection();
  rebuildHierarchyAfterAttachmentChange();
}

void SlintSystem::applyPreviewBehaviourProp(int entity_id, int kind, int index,
                                            int behaviour_id,
                                            const eastl::string& key,
                                            const eastl::string& text, float number,
                                            bool bool_value, bool commit) {
  (void)kind;
  (void)index;
  if (!m_window_component || key.empty()) {
    return;
  }
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  Object* object = scene->findBoundObject(id);
  if (object == nullptr) {
    return;
  }
  const BehaviourId bid = static_cast<BehaviourId>(behaviour_id);
  eastl::vector<BehaviourCatalogType> catalog;
  if (g_runtime_global_context.m_file_system) {
    const auto catalog_path =
        g_runtime_global_context.m_file_system->getProjectRoot() / ".blunder" /
        "behaviour_catalog.json";
    eastl::string error;
    loadBehaviourTypeCatalog(catalog_path, catalog, error);
  }
  const char* type_name = object->getBehaviourTypeName(bid);
  if (type_name == nullptr) {
    return;
  }
  eastl::string resolved_kind = behaviourPropKindFromCatalog(catalog, type_name, key);
  if (resolved_kind.empty()) {
    resolved_kind = "string";
  }
  const Variant after =
      variantFromInspectorCommit(resolved_kind, text, number, bool_value);
  eastl::vector<SceneBehaviourProperty> bag;
  if (const eastl::vector<SceneBehaviourProperty>* existing =
          object->getBehaviourProperties(bid);
      existing != nullptr) {
    bag = *existing;
  }
  Variant before;
  bool found = false;
  for (const SceneBehaviourProperty& prop : bag) {
    if (prop.key == key) {
      before = prop.value;
      found = true;
      break;
    }
  }
  if (!found) {
    if (resolved_kind == "bool") {
      before = Variant(false);
    } else if (resolved_kind == "number") {
      before = Variant(0.0f);
    } else {
      before = Variant(eastl::string{});
    }
  }
  auto write_bag = [&]() {
    bool updated = false;
    for (SceneBehaviourProperty& prop : bag) {
      if (prop.key == key) {
        prop.value = after;
        updated = true;
        break;
      }
    }
    if (!updated) {
      SceneBehaviourProperty prop;
      prop.key = key;
      prop.value = after;
      bag.push_back(eastl::move(prop));
    }
    object->setBehaviourProperties(bid, bag);
  };
  if (!commit) {
    if (!m_inspector_behaviour_edit_open ||
        m_inspector_behaviour_edit_id != bid ||
        m_inspector_behaviour_edit_key != key) {
      m_inspector_behaviour_edit_open = true;
      m_inspector_behaviour_edit_id = bid;
      m_inspector_behaviour_edit_key = key;
      m_inspector_behaviour_edit_before = before;
    }
    if (before != after) {
      write_bag();
    }
    return;
  }
  const Variant command_before =
      (m_inspector_behaviour_edit_open && m_inspector_behaviour_edit_id == bid &&
       m_inspector_behaviour_edit_key == key)
          ? m_inspector_behaviour_edit_before
          : before;
  m_inspector_behaviour_edit_open = false;
  if (before != after) {
    write_bag();
  }
  if (command_before == after) {
    return;
  }
  pushDocumentCommand(makeSetBehaviourPropertyCommand(
      scene, id, bid, key, command_before, after, currentSelectionSnapshot(),
      currentSelectionSnapshot()));
  syncInspectorBehavioursFromSelection();
  syncAttachmentPreviewCards();
}

void SlintSystem::applyPreviewModifierRemove(int entity_id, int kind, int index) {
  (void)kind;
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  Object* object = scene->findBoundObject(id);
  if (object == nullptr || index < 0 ||
      static_cast<size_t>(index) >= object->getSkeletonModifierCount()) {
    return;
  }
  SceneSkeletonModifierDef snapshot;
  if (!captureSkeletonModifierDef(*scene, *object, static_cast<size_t>(index),
                                  snapshot)) {
    return;
  }
  if (!object->removeSkeletonModifierAt(static_cast<size_t>(index))) {
    return;
  }
  pushDocumentCommand(makeRemoveSkeletonModifierCommand(
      scene, id, static_cast<size_t>(index), snapshot, currentSelectionSnapshot(),
      currentSelectionSnapshot()));
  syncInspectorSkeletonModifiersFromSelection();
  notifyAnimationPreviewAfterSkeletonModifierEdit(
      services && services->render_system ? services->render_system.get() : nullptr);
  rebuildHierarchyAfterAttachmentChange();
}

void SlintSystem::applyPreviewModifierEnabled(int entity_id, int kind, int index,
                                              bool enabled) {
  (void)kind;
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  Object* object = scene->findBoundObject(id);
  if (object == nullptr || index < 0 ||
      static_cast<size_t>(index) >= object->getSkeletonModifierCount()) {
    return;
  }
  SkeletonModifier* modifier =
      object->getSkeletonModifierAt(static_cast<size_t>(index));
  if (modifier == nullptr) {
    return;
  }
  const bool before = modifier->isEnabled();
  if (before == enabled) {
    return;
  }
  modifier->setEnabled(enabled);
  pushDocumentCommand(makeSetSkeletonModifierEnabledCommand(
      scene, id, static_cast<size_t>(index), before, enabled,
      currentSelectionSnapshot(), currentSelectionSnapshot()));
  syncInspectorSkeletonModifiersFromSelection();
  notifyAnimationPreviewAfterSkeletonModifierEdit(
      services && services->render_system ? services->render_system.get() : nullptr);
}

void SlintSystem::applyPreviewModifierField(int entity_id, int kind, int index,
                                            const eastl::string& key,
                                            const eastl::string& text, float number,
                                            bool bool_value, bool commit) {
  (void)kind;
  if (key.empty()) {
    return;
  }
  const auto services = lockServices();
  SceneInstance* scene =
      services && services->scene ? services->scene->getActiveInstance() : nullptr;
  if (scene == nullptr) {
    return;
  }
  const EntityId id = static_cast<EntityId>(entity_id);
  Object* object = scene->findBoundObject(id);
  if (object == nullptr || index < 0 ||
      static_cast<size_t>(index) >= object->getSkeletonModifierCount()) {
    return;
  }
  SceneSkeletonModifierDef before_def;
  if (!captureSkeletonModifierDef(*scene, *object, static_cast<size_t>(index),
                                  before_def)) {
    return;
  }
  SceneSkeletonModifierDef after_def = before_def;
  if (key == "bone_name") {
    after_def.bone_name = text;
  } else if (key == "open_amount") {
    after_def.open_amount = number;
  } else if (key == "attach_driven") {
    after_def.attach_driven = bool_value;
  } else if (key == "child_entity_name") {
    after_def.child_entity_name = text;
  } else if (key == "target_x") {
    after_def.target.x = number;
  } else if (key == "target_y") {
    after_def.target.y = number;
  } else if (key == "target_z") {
    after_def.target.z = number;
  } else {
    return;
  }
  const auto defs_equal = [](const SceneSkeletonModifierDef& a,
                             const SceneSkeletonModifierDef& b) {
    return a.type == b.type && a.enabled == b.enabled && a.bone_name == b.bone_name &&
           a.open_amount == b.open_amount && a.attach_driven == b.attach_driven &&
           a.target == b.target && a.child_entity_name == b.child_entity_name;
  };
  const size_t modifier_index = static_cast<size_t>(index);
  if (defs_equal(before_def, after_def) && !m_inspector_modifier_edit_open) {
    return;
  }
  if (!commit) {
    if (!m_inspector_modifier_edit_open ||
        m_inspector_modifier_edit_index != modifier_index ||
        m_inspector_modifier_edit_key != key) {
      m_inspector_modifier_edit_open = true;
      m_inspector_modifier_edit_index = modifier_index;
      m_inspector_modifier_edit_key = key;
      m_inspector_modifier_edit_before = before_def;
    }
    if (!defs_equal(before_def, after_def)) {
      applySkeletonModifierFieldsOnObject(scene, object, modifier_index, after_def);
      notifyAnimationPreviewAfterSkeletonModifierEdit(
          services && services->render_system ? services->render_system.get() : nullptr);
    }
    return;
  }
  const SceneSkeletonModifierDef command_before =
      (m_inspector_modifier_edit_open &&
       m_inspector_modifier_edit_index == modifier_index &&
       m_inspector_modifier_edit_key == key)
          ? m_inspector_modifier_edit_before
          : before_def;
  m_inspector_modifier_edit_open = false;
  if (!defs_equal(before_def, after_def)) {
    applySkeletonModifierFieldsOnObject(scene, object, modifier_index, after_def);
  }
  if (defs_equal(command_before, after_def)) {
    return;
  }
  pushDocumentCommand(makeSetSkeletonModifierDefCommand(
      scene, id, modifier_index, command_before, after_def,
      currentSelectionSnapshot(), currentSelectionSnapshot()));
  syncInspectorSkeletonModifiersFromSelection();
  notifyAnimationPreviewAfterSkeletonModifierEdit(
      services && services->render_system ? services->render_system.get() : nullptr);
  syncAttachmentPreviewCards();
}

}  // namespace Blunder
