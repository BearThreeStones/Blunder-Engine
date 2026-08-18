#include "EASTL/unique_ptr.h"
#include "runtime/function/scene/scene_instance.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/animation_tree_asset.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/object/missing_skeleton_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/function/editor/animation_clip_resolve.h"
#include "runtime/function/editor/inspector_skeleton_modifier_ops.h"
#include "runtime/function/scene/scene_serializer.h"

#include <cstddef>
#include <cstring>

#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {

namespace {

Mat4 composeTrs(const Vec3& position, const Quat& rotation, const Vec3& scale) {
  const Mat4 translation = glm::translate(Mat4(1.0f), position);
  const Mat4 rot = glm::mat4_cast(rotation);
  const Mat4 scl = glm::scale(Mat4(1.0f), scale);
  return translation * rot * scl;
}

bool entityHasAnimationTreeTopology(const SceneEntityDefinition& definition) {
  return definition.has_animation_tree ||
         !definition.animation_tree_asset_guid.empty() ||
         !definition.animation_tree_blend_spaces.empty() ||
         !definition.animation_tree_states.empty() ||
         !definition.animation_tree_current_state.empty() ||
         !definition.animation_tree_base_blend_space_node.empty() ||
         !definition.animation_tree_add2_clip.empty() ||
         !definition.animation_tree_oneshot_clip.empty() ||
         definition.animation_tree_active ||
         definition.animation_tree_add2_weight != 0.0f;
}

void applyAnimationTreeTopology(AnimationTree& tree,
                                const SceneEntityDefinition& definition) {
  if (!definition.animation_tree_asset_guid.empty()) {
    tree.setAssetGuid(definition.animation_tree_asset_guid);
    // Asset body is applied by the host/resolver; scene stores allowlisted
    // overrides only (scalars / Add2 weight / travel / active).
    AnimationTreeInstanceOverrides overrides;
    for (const SceneEntityDefinition::AnimationTreeBlendSpaceDef& space :
         definition.animation_tree_blend_spaces) {
      overrides.blend_space_scalars.push_back(
          {space.node_name, space.scalar});
    }
    if (definition.animation_tree_add2_weight != 0.0f) {
      overrides.has_add2_weight = true;
      overrides.add2_weight = definition.animation_tree_add2_weight;
    }
    overrides.current_state = definition.animation_tree_current_state;
    if (definition.animation_tree_active) {
      overrides.has_active = true;
      overrides.active = true;
    }
    applyAnimationTreeInstanceOverrides(tree, overrides);
    return;
  }

  for (const SceneEntityDefinition::AnimationTreeBlendSpaceDef& space :
       definition.animation_tree_blend_spaces) {
    for (const SceneEntityDefinition::AnimationTreeBlendSpacePointDef& point :
         space.points) {
      tree.addBlendSpacePoint(space.node_name, point.clip_name, point.scalar);
    }
    tree.setBlendSpaceScalar(space.node_name, space.scalar);
  }
  for (const SceneEntityDefinition::AnimationTreeStateDef& state :
       definition.animation_tree_states) {
    if (state.kind == "blendSpace1D") {
      tree.setStateBlendSpace(state.name, state.blend_space_node);
    } else if (state.kind == "blendSpace2D") {
      tree.setStateBlendSpace2D(state.name, state.blend_space_node);
    } else {
      tree.setStateClip(state.name, state.clip_name);
    }
  }
  if (!definition.animation_tree_base_blend_space_node.empty()) {
    tree.setBaseBlendSpaceNode(definition.animation_tree_base_blend_space_node);
  }
  if (!definition.animation_tree_add2_clip.empty()) {
    tree.setAdd2ClipName(definition.animation_tree_add2_clip);
    tree.setAdd2Weight(definition.animation_tree_add2_weight);
  }
  if (!definition.animation_tree_oneshot_clip.empty()) {
    tree.setOneShotSlotClip(definition.animation_tree_oneshot_clip);
  }
  if (!definition.animation_tree_current_state.empty()) {
    tree.travel(definition.animation_tree_current_state);
  }
  if (definition.animation_tree_active) {
    tree.setActive(true);
  }
}

void captureAnimationTreeTopology(const AnimationTree& tree,
                                  SceneEntityDefinition& definition) {
  definition.has_animation_tree = true;
  definition.animation_tree_asset_guid = tree.getAssetGuid();
  definition.animation_tree_active = tree.isActive();
  definition.animation_tree_current_state = tree.getCurrentStateName();
  definition.animation_tree_base_blend_space_node =
      tree.getBaseBlendSpaceNode();
  definition.animation_tree_add2_clip = tree.getAdd2ClipName();
  definition.animation_tree_add2_weight = tree.getAdd2Weight();
  definition.animation_tree_oneshot_clip = tree.getOneShotSlotClip();

  // When referencing an Asset, persist allowlisted overrides only.
  if (!definition.animation_tree_asset_guid.empty()) {
    struct OverrideExportContext {
      SceneEntityDefinition* definition;
    };
    OverrideExportContext ctx{&definition};
    tree.visitBlendSpaces(
        [](const eastl::string& node_name,
           const eastl::vector<BlendSpace1DPoint>& /*points*/, float scalar,
           void* userdata) {
          auto* export_ctx = static_cast<OverrideExportContext*>(userdata);
          SceneEntityDefinition::AnimationTreeBlendSpaceDef space;
          space.node_name = node_name;
          space.scalar = scalar;
          export_ctx->definition->animation_tree_blend_spaces.push_back(
              eastl::move(space));
        },
        &ctx);
    return;
  }
  struct BlendExportContext {
    SceneEntityDefinition* definition;
  };
  BlendExportContext blend_ctx{&definition};
  tree.visitBlendSpaces(
      [](const eastl::string& node_name,
         const eastl::vector<BlendSpace1DPoint>& points, float scalar,
         void* userdata) {
        auto* ctx = static_cast<BlendExportContext*>(userdata);
        SceneEntityDefinition::AnimationTreeBlendSpaceDef space;
        space.node_name = node_name;
        space.scalar = scalar;
        for (const BlendSpace1DPoint& point : points) {
          SceneEntityDefinition::AnimationTreeBlendSpacePointDef point_def;
          point_def.clip_name = point.clip_name;
          point_def.scalar = point.scalar;
          space.points.push_back(eastl::move(point_def));
        }
        ctx->definition->animation_tree_blend_spaces.push_back(
            eastl::move(space));
      },
      &blend_ctx);

  struct StateExportContext {
    SceneEntityDefinition* definition;
  };
  StateExportContext state_ctx{&definition};
  tree.visitStates(
      [](const eastl::string& state_name, AnimationStatePlaybackKind kind,
         const eastl::string& clip_name, const eastl::string& blend_space_node,
         void* userdata) {
        auto* ctx = static_cast<StateExportContext*>(userdata);
        SceneEntityDefinition::AnimationTreeStateDef state_def;
        state_def.name = state_name;
        if (kind == AnimationStatePlaybackKind::BlendSpace1D) {
          state_def.kind = "blendSpace1D";
          state_def.blend_space_node = blend_space_node;
        } else if (kind == AnimationStatePlaybackKind::BlendSpace2D) {
          state_def.kind = "blendSpace2D";
          state_def.blend_space_node = blend_space_node;
        } else {
          state_def.kind = "clip";
          state_def.clip_name = clip_name;
        }
        ctx->definition->animation_tree_states.push_back(
            eastl::move(state_def));
      },
      &state_ctx);
}

/// Rebuilds one chain slot from its definition. Attach children are wired in a
/// later pass because the child entity may not exist yet.
void applySkeletonModifierDefinition(Object& object,
                                     const SceneSkeletonModifierDef& def) {
  eastl::unique_ptr<SkeletonModifier> created = makeSkeletonModifierFromDef(def);
  if (created == nullptr) {
    return;
  }
  object.addSkeletonModifier(eastl::move(created));
}

void captureSkeletonModifiers(const SceneInstance& scene, const Object& object,
                              SceneEntityDefinition& definition) {
  const size_t count = object.getSkeletonModifierCount();
  definition.skeleton_modifiers.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const SkeletonModifier* modifier = object.getSkeletonModifierAt(i);
    if (modifier == nullptr) {
      continue;
    }
    SceneSkeletonModifierDef def;
    def.type = modifier->getTypeName();
    def.enabled = modifier->isEnabled();

    if (modifier->isMissing()) {
      const auto* missing =
          static_cast<const MissingSkeletonModifier*>(modifier);
      def.extra_fields = missing->extraFields();
      definition.skeleton_modifiers.push_back(eastl::move(def));
      continue;
    }

    if (def.type == "PaperMouth") {
      const auto* mouth =
          static_cast<const SkeletonPaperMouthModifier*>(modifier);
      def.bone_name = mouth->getBoneName();
      def.open_amount = mouth->getOpenAmount();
      def.attach_driven = mouth->isAttachDriven();
    } else if (def.type == "SkeletonLookAtModifier") {
      const auto* look_at =
          static_cast<const SkeletonLookAtModifier*>(modifier);
      def.bone_name = look_at->getBoneName();
      def.target = look_at->getTarget();
    } else if (def.type == "SkeletonAttachModifier") {
      const auto* attach =
          static_cast<const SkeletonAttachModifier*>(modifier);
      def.bone_name = attach->getBoneName();
      // ObjectId is a session handle: persist the child by entity name.
      if (const Object* child = ObjectDB::get(attach->getChildObjectId())) {
        const Entity* child_entity = scene.getEntity(child->getEntityId());
        def.child_entity_name = child_entity != nullptr
                                    ? child_entity->getName()
                                    : child->getName();
      }
    }

    definition.skeleton_modifiers.push_back(eastl::move(def));
  }
}

}  // namespace

SceneInstance::~SceneInstance() { clear(); }

void SceneInstance::instantiate(const Scene& scene) {
  clear();

  eastl::vector<EntityId> ids;
  ids.reserve(scene.getEntities().size());

  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    const EntityId id = createEntity(definition.name, definition.position,
                                     definition.rotation, definition.scale);
    ids.push_back(id);
    if (!definition.mesh_virtual_path.empty()) {
      if (Entity* entity = getEntity(id)) {
        entity->setMeshVirtualPath(definition.mesh_virtual_path);
      }
    }
    // Bind Object + restore Behaviour slots only when the list is non-empty.
    // Peers stay null here; mountSceneBehaviours attaches when DotNetHost runs.
    const bool needs_object =
        !definition.behaviours.empty() || definition.has_skeleton ||
        !definition.animation_player_clips.empty() ||
        !definition.skeleton_modifiers.empty() ||
        entityHasAnimationTreeTopology(definition);
    if (needs_object) {
      const ObjectId object_id = ObjectDB::create();
      Object* object = ObjectDB::get(object_id);
      if (object == nullptr) {
        LOG_ERROR("[SceneInstance] failed to create Object for entity '{}'",
                  definition.name.c_str());
        continue;
      }
      object->setName(definition.name);
      object->setEntityId(id);
      m_bound_object_ids.push_back(object_id);
      if (definition.has_skeleton) {
        object->ensureSkeleton();
      }
      if (!definition.animation_player_clips.empty() ||
          entityHasAnimationTreeTopology(definition)) {
        AnimationPlayer* player = object->ensureAnimationPlayer();
        wireAnimationPlayerAssetResolver(*player);
        for (const SceneEntityDefinition::AnimationClipBinding& binding :
             definition.animation_player_clips) {
          player->setClipGuid(binding.name, binding.guid);
          if (m_default_animation_clip_names.find(id) ==
              m_default_animation_clip_names.end()) {
            m_default_animation_clip_names[id] = binding.name;
          }
        }
        if (!definition.animation_player_clips.empty()) {
          player->setTimeScale(definition.animation_player_time_scale);
          if (!definition.animation_player_slot0.empty()) {
            player->setSlot(0, definition.animation_player_slot0);
          }
          if (!definition.animation_player_slot1.empty()) {
            player->setSlot(1, definition.animation_player_slot1);
          }
          player->setBlendWeight(definition.animation_player_blend_weight);
        }
      }
      if (entityHasAnimationTreeTopology(definition)) {
        AnimationTree* tree = object->ensureAnimationTree();
        applyAnimationTreeTopology(*tree, definition);
      }
      for (const SceneBehaviourDeclaration& decl : definition.behaviours) {
        if (!object->restoreBehaviour(decl.id, decl.type)) {
          LOG_WARN(
              "[SceneInstance] skipped Behaviour restore id={} type='{}' on '{}'",
              static_cast<unsigned long long>(decl.id), decl.type.c_str(),
              definition.name.c_str());
          continue;
        }
        object->setBehaviourProperties(decl.id, decl.properties);
      }
      for (const SceneSkeletonModifierDef& modifier_def :
           definition.skeleton_modifiers) {
        applySkeletonModifierDefinition(*object, modifier_def);
      }
    }
  }

  // Second pass: Attach children are named entities that may appear after their
  // host, so bind them only once every entity exists.
  for (size_t i = 0; i < scene.getEntities().size(); ++i) {
    const SceneEntityDefinition& definition = scene.getEntities()[i];
    if (definition.skeleton_modifiers.empty()) {
      continue;
    }
    Object* host = findBoundObject(ids[i]);
    if (host == nullptr) {
      continue;
    }
    for (size_t mi = 0; mi < definition.skeleton_modifiers.size(); ++mi) {
      const SceneSkeletonModifierDef& def = definition.skeleton_modifiers[mi];
      if (def.type != "SkeletonAttachModifier" ||
          def.child_entity_name.empty()) {
        continue;
      }
      const auto child_it = m_name_to_id.find(def.child_entity_name);
      if (child_it == m_name_to_id.end()) {
        LOG_WARN("[SceneInstance] Attach child '{}' not found for entity '{}'",
                 def.child_entity_name.c_str(), definition.name.c_str());
        continue;
      }
      // The child needs an Object to receive the bone transform even when it
      // carries no Behaviour / Skeleton of its own.
      Object* child = ensureBoundObject(child_it->second);
      SkeletonModifier* slot = host->getSkeletonModifierAt(mi);
      if (child == nullptr || slot == nullptr ||
          std::strcmp(slot->getTypeName(), "SkeletonAttachModifier") != 0) {
        continue;
      }
      static_cast<SkeletonAttachModifier*>(slot)->setChildObjectId(
          child->getId());
    }
  }

  for (size_t i = 0; i < scene.getEntities().size(); ++i) {
    const SceneEntityDefinition& definition = scene.getEntities()[i];
    if (definition.parent_name.empty()) {
      continue;
    }

    const auto parent_it = m_name_to_id.find(definition.parent_name);
    if (parent_it == m_name_to_id.end()) {
      LOG_WARN("[SceneInstance] entity '{}' parent '{}' not found in scene '{}'",
               definition.name.c_str(), definition.parent_name.c_str(),
               m_source_path.c_str());
      continue;
    }

    Entity* entity = getEntity(ids[i]);
    if (entity != nullptr) {
      entity->setParentId(parent_it->second);
    }
  }

  if (!validateParentChains()) {
    LOG_ERROR("[SceneInstance] invalid parent chain in scene '{}'",
              m_source_path.c_str());
  }

  m_world_matrices_dirty = true;
  rebuildWorldMatrices();
}

void SceneInstance::clear() {
  // Destroy Objects bound for Behaviour slots so findByEntityId cannot return
  // stale process-global entries after re-instantiate (EntityId is local).
  for (ObjectId object_id : m_bound_object_ids) {
    ObjectDB::destroy(object_id);
  }
  m_bound_object_ids.clear();
  m_default_animation_clip_names.clear();

  m_entities.clear();
  m_world_matrices.clear();
  m_name_to_id.clear();
  m_mesh_renderers.clear();
  m_cameras.clear();
  m_has_world_bounds = false;
  m_world_bounds = AABB{};
  m_world_matrices_dirty = true;
}

void SceneInstance::setMeshRenderer(EntityId id, MeshRendererComponent renderer) {
  if (!isValid(id)) {
    return;
  }
  m_mesh_renderers[id] = eastl::move(renderer);
}

const MeshRendererComponent* SceneInstance::getMeshRenderer(EntityId id) const {
  const auto it = m_mesh_renderers.find(id);
  if (it == m_mesh_renderers.end()) {
    return nullptr;
  }
  return &it->second;
}

void SceneInstance::setCamera(EntityId id, CameraComponent camera) {
  if (!isValid(id)) {
    return;
  }
  m_cameras[id] = eastl::move(camera);
}

const CameraComponent* SceneInstance::getCamera(EntityId id) const {
  const auto it = m_cameras.find(id);
  if (it == m_cameras.end()) {
    return nullptr;
  }
  return &it->second;
}

void SceneInstance::clearCamera(EntityId id) {
  if (!isValid(id)) {
    return;
  }
  m_cameras.erase(id);
}

void SceneInstance::setWorldBounds(const AABB& bounds) {
  m_world_bounds = bounds;
  m_has_world_bounds = true;
}

void SceneInstance::setParent(SceneInstance* parent) {
  m_parent_instance = parent;
  m_world_matrices_dirty = true;
}

void SceneInstance::setRootTransform(const Vec3& position, const Quat& rotation,
                                     const Vec3& scale) {
  m_root_position = position;
  m_root_rotation = rotation;
  m_root_scale = scale;
  m_world_matrices_dirty = true;
}

Mat4 SceneInstance::getSceneToWorldMatrix() const {
  const Mat4 local_root =
      composeTrs(m_root_position, m_root_rotation, m_root_scale);
  if (m_parent_instance == nullptr) {
    return local_root;
  }
  return m_parent_instance->getSceneToWorldMatrix() * local_root;
}

EntityId SceneInstance::createEntity(eastl::string name, const Vec3& position,
                                     const Quat& rotation, const Vec3& scale,
                                     EntityId parent_id) {
  Entity entity;
  entity.setName(eastl::move(name));
  entity.setPosition(position);
  entity.setRotation(rotation);
  entity.setScale(scale);
  entity.setParentId(parent_id);

  m_entities.push_back(entity);
  const EntityId id = indexToId(m_entities.size() - 1u);
  if (!m_entities.back().getName().empty()) {
    m_name_to_id[m_entities.back().getName()] = id;
  }

  m_world_matrices.resize(m_entities.size(), Mat4(1.0f));
  m_world_matrices_dirty = true;
  return id;
}

bool SceneInstance::softDeleteEntity(EntityId id) {
  Entity* entity = getEntity(id);
  if (entity == nullptr || entity->isTombstoned()) {
    return false;
  }
  entity->setTombstoned(true);
  entity->setEnabled(false);
  m_world_matrices_dirty = true;
  return true;
}

bool SceneInstance::restoreEntity(EntityId id) {
  Entity* entity = getEntity(id);
  if (entity == nullptr || !entity->isTombstoned()) {
    return false;
  }
  entity->setTombstoned(false);
  entity->setEnabled(true);
  m_world_matrices_dirty = true;
  return true;
}

bool SceneInstance::isTombstoned(EntityId id) const {
  const Entity* entity = getEntity(id);
  return entity != nullptr && entity->isTombstoned();
}

bool SceneInstance::isOmittedFromDocument(EntityId id) const {
  EntityId current = id;
  while (isValid(current)) {
    if (isTombstoned(current)) {
      return true;
    }
    const Entity* entity = getEntity(current);
    if (entity == nullptr) {
      break;
    }
    current = entity->getParentId();
  }
  return false;
}

const Entity* SceneInstance::getEntity(EntityId id) const {
  if (!isValid(id)) {
    return nullptr;
  }
  const size_t index = idToIndex(id);
  if (index >= m_entities.size()) {
    return nullptr;
  }
  return &m_entities[index];
}

Entity* SceneInstance::getEntity(EntityId id) {
  if (!isValid(id)) {
    return nullptr;
  }
  const size_t index = idToIndex(id);
  if (index >= m_entities.size()) {
    return nullptr;
  }
  return &m_entities[index];
}

EntityId SceneInstance::findEntityByName(const eastl::string& name) const {
  const auto it = m_name_to_id.find(name);
  if (it == m_name_to_id.end()) {
    return k_invalid_entity_id;
  }
  return it->second;
}

EntityId SceneInstance::getEntityIdAtIndex(size_t index) const {
  if (index >= m_entities.size()) {
    return k_invalid_entity_id;
  }
  return indexToId(index);
}

bool SceneInstance::exportToScene(Scene& out_scene) const {
  out_scene.getEntities().clear();

  for (size_t i = 0; i < m_entities.size(); ++i) {
    const Entity& entity = m_entities[i];
    if (isOmittedFromDocument(indexToId(i))) {
      continue;
    }
    SceneEntityDefinition definition;
    definition.name = entity.getName();
    definition.position = entity.getPosition();
    definition.rotation = entity.getRotation();
    definition.scale = entity.getScale();
    definition.mesh_virtual_path = entity.getMeshVirtualPath();

    const EntityId parent_id = entity.getParentId();
    if (isValid(parent_id)) {
      const Entity* parent = getEntity(parent_id);
      if (parent != nullptr && !parent->isTombstoned()) {
        definition.parent_name = parent->getName();
      }
    }

    // Prefer SceneInstance-tracked ObjectIds over a process-global EntityId scan.
    const EntityId entity_id = indexToId(i);
    Object* bound = nullptr;
    for (ObjectId object_id : m_bound_object_ids) {
      Object* candidate = ObjectDB::get(object_id);
      if (candidate != nullptr && candidate->getEntityId() == entity_id) {
        bound = candidate;
        break;
      }
    }
    if (bound != nullptr) {
      if (bound->hasSkeleton()) {
        definition.has_skeleton = true;
      }
      if (AnimationPlayer* player = bound->getAnimationPlayer()) {
        for (const AnimationPlayer::ClipBinding& binding :
             player->getClipBindings()) {
          SceneEntityDefinition::AnimationClipBinding clip_binding;
          clip_binding.name = binding.name;
          clip_binding.guid = binding.guid;
          definition.animation_player_clips.push_back(eastl::move(clip_binding));
        }
      }
      const size_t behaviour_count = bound->getBehaviourCount();
      definition.behaviours.reserve(behaviour_count);
      for (size_t bi = 0; bi < behaviour_count; ++bi) {
        const BehaviourId behaviour_id = bound->getBehaviourIdAt(bi);
        const char* type_name = bound->getBehaviourTypeName(behaviour_id);
        SceneBehaviourDeclaration decl;
        decl.id = behaviour_id;
        decl.type = type_name != nullptr ? type_name : "";
        if (const eastl::vector<SceneBehaviourProperty>* properties =
                bound->getBehaviourProperties(behaviour_id);
            properties != nullptr) {
          decl.properties = *properties;
        }
        definition.behaviours.push_back(eastl::move(decl));
      }

      definition.has_skeleton = bound->hasSkeleton();
      if (AnimationPlayer* player = bound->getAnimationPlayer()) {
        struct ClipExportContext {
          SceneEntityDefinition* definition;
        };
        ClipExportContext ctx{&definition};
        player->visitClipBindings(
            [](const eastl::string& name, const eastl::string& guid,
               void* userdata) {
              auto* export_ctx = static_cast<ClipExportContext*>(userdata);
              SceneEntityDefinition::AnimationClipBinding binding;
              binding.name = name;
              binding.guid = guid;
              export_ctx->definition->animation_player_clips.push_back(
                  eastl::move(binding));
            },
            &ctx);
        definition.animation_player_time_scale = player->getTimeScale();
        definition.animation_player_slot0 = player->getSlotClipName(0);
        definition.animation_player_slot1 = player->getSlotClipName(1);
        definition.animation_player_blend_weight = player->getBlendWeight();
      }
      if (AnimationTree* tree = bound->getAnimationTree()) {
        captureAnimationTreeTopology(*tree, definition);
      }
      captureSkeletonModifiers(*this, *bound, definition);
    }

    if (const CameraComponent* camera = getCamera(entity_id)) {
      definition.has_camera = true;
      definition.camera = *camera;
    }

    out_scene.getEntities().push_back(eastl::move(definition));
  }

  return true;
}

Mat4 SceneInstance::getWorldMatrix(EntityId id) const {
  if (!isValid(id)) {
    return Mat4(1.0f);
  }
  const size_t index = idToIndex(id);
  if (index >= m_world_matrices.size()) {
    return Mat4(1.0f);
  }
  return m_world_matrices[index];
}

void SceneInstance::tick(float delta_time) {
  (void)delta_time;
  if (m_world_matrices_dirty) {
    rebuildWorldMatrices();
  }
}

eastl::string SceneInstance::getDefaultAnimationClipName(
    EntityId entity_id) const {
  const auto it = m_default_animation_clip_names.find(entity_id);
  if (it == m_default_animation_clip_names.end()) {
    return eastl::string();
  }
  return it->second;
}

Object* SceneInstance::findBoundObject(EntityId entity_id) const {
  if (!isValid(entity_id)) {
    return nullptr;
  }
  for (ObjectId object_id : m_bound_object_ids) {
    Object* object = ObjectDB::get(object_id);
    if (object != nullptr && object->getEntityId() == entity_id) {
      return object;
    }
  }
  return nullptr;
}

Object* SceneInstance::ensureBoundObject(EntityId entity_id) {
  if (Object* existing = findBoundObject(entity_id)) {
    return existing;
  }
  if (!isValid(entity_id)) {
    return nullptr;
  }
  Entity* entity = getEntity(entity_id);
  if (entity == nullptr || entity->isTombstoned()) {
    return nullptr;
  }

  const ObjectId object_id = ObjectDB::create();
  Object* object = ObjectDB::get(object_id);
  if (object == nullptr) {
    LOG_ERROR("[SceneInstance] failed to create Object for entity '{}'",
              entity->getName().c_str());
    return nullptr;
  }
  object->setName(entity->getName());
  object->setEntityId(entity_id);
  m_bound_object_ids.push_back(object_id);
  return object;
}

void SceneInstance::releaseBoundObject(EntityId entity_id) {
  Object* object = findBoundObject(entity_id);
  if (object == nullptr) {
    return;
  }
  const ObjectId object_id = object->getId();
  for (size_t index = 0; index < m_bound_object_ids.size(); ++index) {
    if (m_bound_object_ids[index] == object_id) {
      m_bound_object_ids.erase(m_bound_object_ids.begin() +
                               static_cast<ptrdiff_t>(index));
      break;
    }
  }
  ObjectDB::destroy(object_id);
}

Skeleton* SceneInstance::findSkeletonForEntity(EntityId entity_id) const {
  EntityId current = entity_id;
  while (isValid(current)) {
    if (Object* object = findBoundObject(current)) {
      if (object->hasSkeleton()) {
        return object->getSkeleton();
      }
    }
    const Entity* entity = getEntity(current);
    if (entity == nullptr) {
      break;
    }
    current = entity->getParentId();
  }
  return nullptr;
}

EntityId SceneInstance::indexToId(size_t index) const {
  return static_cast<EntityId>(index + 1u);
}

size_t SceneInstance::idToIndex(EntityId id) const {
  ASSERT(isValid(id));
  return static_cast<size_t>(id - 1u);
}

bool SceneInstance::validateParentChains() const {
  for (size_t i = 0; i < m_entities.size(); ++i) {
    const EntityId start_id = indexToId(i);
    EntityId current = m_entities[i].getParentId();
    size_t depth = 0;
    while (isValid(current)) {
      if (current == start_id) {
        return false;
      }
      const size_t parent_index = idToIndex(current);
      if (parent_index >= m_entities.size()) {
        return false;
      }
      current = m_entities[parent_index].getParentId();
      ++depth;
      if (depth > m_entities.size()) {
        return false;
      }
    }
  }
  return true;
}

void SceneInstance::rebuildWorldMatrices() {
  const Mat4 scene_to_world = getSceneToWorldMatrix();

  eastl::vector<bool> resolved(m_entities.size(), false);
  bool progress = true;
  while (progress) {
    progress = false;
    for (size_t i = 0; i < m_entities.size(); ++i) {
      if (resolved[i]) {
        continue;
      }

      const Entity& entity = m_entities[i];
      if (!entity.isEnabled()) {
        m_world_matrices[i] = scene_to_world;
        resolved[i] = true;
        progress = true;
        continue;
      }

      Mat4 local_to_scene = entity.getLocalMatrix();
      const EntityId parent_id = entity.getParentId();
      if (isValid(parent_id)) {
        const size_t parent_index = idToIndex(parent_id);
        if (parent_index >= m_entities.size() || !resolved[parent_index]) {
          continue;
        }
        local_to_scene = m_world_matrices[parent_index] * local_to_scene;
        m_world_matrices[i] = local_to_scene;
      } else {
        m_world_matrices[i] = scene_to_world * local_to_scene;
      }

      resolved[i] = true;
      progress = true;
    }
  }

  for (size_t i = 0; i < m_entities.size(); ++i) {
    if (!resolved[i]) {
      LOG_WARN("[SceneInstance] unresolved transform for entity '{}' in '{}'",
               m_entities[i].getName().c_str(), m_source_path.c_str());
      m_world_matrices[i] = scene_to_world * m_entities[i].getLocalMatrix();
    }
  }

  m_world_matrices_dirty = false;
}

}  // namespace Blunder
