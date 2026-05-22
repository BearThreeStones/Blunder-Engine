#include "runtime/function/scene/scene_instance.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/scene/scene_serializer.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {

namespace {

Mat4 composeTrs(const Vec3& position, const Quat& rotation, const Vec3& scale) {
  const Mat4 translation = glm::translate(Mat4(1.0f), position);
  const Mat4 rot = glm::mat4_cast(rotation);
  const Mat4 scl = glm::scale(Mat4(1.0f), scale);
  return translation * rot * scl;
}

}  // namespace

void SceneInstance::instantiate(const Scene& scene) {
  clear();

  eastl::vector<EntityId> ids;
  ids.reserve(scene.getEntities().size());

  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    const EntityId id = createEntity(definition.name, definition.position,
                                     definition.rotation, definition.scale);
    ids.push_back(id);
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
  m_entities.clear();
  m_world_matrices.clear();
  m_name_to_id.clear();
  m_mesh_renderers.clear();
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
    SceneEntityDefinition definition;
    definition.name = entity.getName();
    definition.position = entity.getPosition();
    definition.rotation = entity.getRotation();
    definition.scale = entity.getScale();

    const EntityId parent_id = entity.getParentId();
    if (isValid(parent_id)) {
      const Entity* parent = getEntity(parent_id);
      if (parent != nullptr) {
        definition.parent_name = parent->getName();
      }
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
