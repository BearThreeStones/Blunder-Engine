#include "runtime/function/scene/gltf_scene_importer.h"

#include <cgltf.h>

#include <cstdio>
#include <cstring>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "EASTL/unordered_map.h"
#include "EASTL/unordered_set.h"
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_manager/asset_manager_gltf.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/function/scene/skeleton_from_gltf.h"

namespace Blunder {

namespace {

void expandWorldAabb(AABB& bounds, bool& has_bounds, const glm::vec3& world) {
  if (!has_bounds) {
    bounds.min = world;
    bounds.max = world;
    has_bounds = true;
  } else {
    bounds.expandToInclude(world);
  }
}

void expandBoundsWithMesh(AABB& bounds, bool& has_bounds, const MeshAsset& mesh,
                          const glm::mat4& world_matrix) {
  const AABB& local = mesh.getLocalBounds();
  const glm::vec3 corners[8] = {
      {local.min.x, local.min.y, local.min.z},
      {local.min.x, local.min.y, local.max.z},
      {local.min.x, local.max.y, local.min.z},
      {local.min.x, local.max.y, local.max.z},
      {local.max.x, local.min.y, local.min.z},
      {local.max.x, local.min.y, local.max.z},
      {local.max.x, local.max.y, local.min.z},
      {local.max.x, local.max.y, local.max.z},
  };
  for (const glm::vec3& corner : corners) {
    expandWorldAabb(bounds, has_bounds,
                    glm::vec3(world_matrix * glm::vec4(corner, 1.0f)));
  }
}

EntityId findUnusedDirectChild(SceneInstance& scene, EntityId parent_id,
                               const eastl::string& name,
                               const eastl::unordered_set<EntityId>& used) {
  EntityId found = k_invalid_entity_id;
  if (!isValid(parent_id) || name.empty()) {
    return found;
  }
  scene.forEachChild(parent_id, [&](EntityId id, const Entity& entity) {
    if (isValid(found) || entity.isTombstoned()) {
      return;
    }
    if (used.find(id) != used.end()) {
      return;
    }
    if (entity.getName() == name) {
      found = id;
    }
  });
  return found;
}

bool isEntityOrDescendantOf(const SceneInstance& scene, EntityId root,
                            EntityId candidate) {
  EntityId current = candidate;
  while (isValid(current)) {
    if (current == root) {
      return true;
    }
    const Entity* entity = scene.getEntity(current);
    if (entity == nullptr) {
      break;
    }
    current = entity->getParentId();
  }
  return false;
}

EntityId findUnusedDescendant(SceneInstance& scene, EntityId root,
                              const eastl::string& name,
                              const eastl::unordered_set<EntityId>& used) {
  EntityId found = k_invalid_entity_id;
  if (!isValid(root) || name.empty()) {
    return found;
  }
  scene.forEachEntity([&](EntityId id, const Entity& entity) {
    if (isValid(found) || entity.isTombstoned() || id == root) {
      return;
    }
    if (used.find(id) != used.end() || entity.getName() != name) {
      return;
    }
    if (isEntityOrDescendantOf(scene, root, id)) {
      found = id;
    }
  });
  return found;
}

EntityId findReusableNamedEntity(SceneInstance& scene, EntityId attach_root,
                                 EntityId parent_id, const eastl::string& name,
                                 const eastl::unordered_set<EntityId>& used) {
  EntityId found = findUnusedDirectChild(scene, parent_id, name, used);
  if (isValid(found)) {
    return found;
  }
  return findUnusedDescendant(scene, attach_root, name, used);
}

GltfSceneImporter::ImportResult importGltfDocument(
    AssetManager* asset_manager, GltfImportDocument& document,
    SceneInstance& scene_instance, EntityId attach_parent_entity) {
  GltfSceneImporter::ImportResult result{};
  cgltf_data* data = document.data;
  if (data == nullptr) {
    result.error_message = "glTF document is not open";
    return result;
  }
  const std::filesystem::path& absolute = document.absolute;
  const eastl::string& gltf_key = document.canonical_key;
  eastl::vector<EntityId> new_primitive_entities;
  eastl::unordered_set<EntityId> reused_ids;

  const auto visit_node = [&](const auto& visit_self, cgltf_node* node,
                              EntityId parent_entity_id) -> void {
    if (node == nullptr) {
      return;
    }

    const eastl::string node_name = gltfNodeDisplayName(node);
    EntityId node_entity_id = findReusableNamedEntity(
        scene_instance, attach_parent_entity, parent_entity_id, node_name,
        reused_ids);
    if (isValid(node_entity_id)) {
      reused_ids.insert(node_entity_id);
    } else {
      Vec3 local_position{};
      Quat local_rotation = glm::identity<Quat>();
      Vec3 local_scale(1.0f);
      decomposeCgltfNodeLocal(node, local_position, local_rotation, local_scale);
      node_entity_id = scene_instance.createEntity(
          node_name, local_position, local_rotation, local_scale,
          parent_entity_id);
    }

    if (node->skin != nullptr) {
      Object* skin_object = scene_instance.ensureBoundObject(node_entity_id);
      if (skin_object != nullptr) {
        Skeleton* skeleton = skin_object->ensureSkeleton();
        if (skeleton->getBoneCount() == 0) {
          populateSkeletonFromSkin(node->skin, *skeleton);
        }
      }
    }

    if (node->mesh != nullptr) {
      const size_t mesh_index = static_cast<size_t>(node->mesh - data->meshes);
      const cgltf_mesh& mesh = *node->mesh;
      for (cgltf_size primitive_index = 0;
           primitive_index < mesh.primitives_count; ++primitive_index) {
        const eastl::shared_ptr<MeshAsset> mesh_asset =
            asset_manager->loadMeshPrimitive(data, mesh_index,
                                             static_cast<size_t>(primitive_index),
                                             absolute, gltf_key, node->skin);
        if (!mesh_asset) {
          continue;
        }

        MeshRendererComponent renderer{};
        renderer.mesh = mesh_asset;
        renderer.material = mesh_asset->getMaterialAsset();
        if (renderer.material) {
          renderer.alpha_mode = renderer.material->getAlphaMode();
          renderer.alpha_cutoff = renderer.material->getAlphaCutoff();
          renderer.double_sided = renderer.material->isDoubleSided();
        }

        char primitive_name[128];
        std::snprintf(primitive_name, sizeof(primitive_name), "%s_prim%zu",
                      node_name.c_str(),
                      static_cast<size_t>(primitive_index));
        EntityId primitive_entity_id = findReusableNamedEntity(
            scene_instance, attach_parent_entity, node_entity_id,
            eastl::string(primitive_name), reused_ids);
        if (isValid(primitive_entity_id)) {
          reused_ids.insert(primitive_entity_id);
        } else {
          primitive_entity_id = scene_instance.createEntity(
              primitive_name, Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f),
              node_entity_id);
        }
        scene_instance.setMeshRenderer(primitive_entity_id, eastl::move(renderer));
        new_primitive_entities.push_back(primitive_entity_id);
        ++result.mesh_primitive_count;
      }
    }

    for (cgltf_size child_index = 0; child_index < node->children_count;
         ++child_index) {
      visit_self(visit_self, node->children[child_index], node_entity_id);
    }
  };

  const EntityId root_parent =
      isValid(attach_parent_entity) ? attach_parent_entity : k_invalid_entity_id;

  if (data->scene != nullptr && data->scene->nodes_count > 0) {
    for (cgltf_size root_index = 0; root_index < data->scene->nodes_count;
         ++root_index) {
      visit_node(visit_node, data->scene->nodes[root_index], root_parent);
    }
  } else {
    for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
      cgltf_node* node = &data->nodes[node_index];
      if (node->parent == nullptr) {
        visit_node(visit_node, node, root_parent);
      }
    }
  }

  scene_instance.markTransformsDirty();
  scene_instance.tick(0.0f);

  for (const EntityId entity_id : new_primitive_entities) {
    const MeshRendererComponent* renderer = scene_instance.getMeshRenderer(entity_id);
    if (renderer == nullptr || !renderer->mesh) {
      continue;
    }
    expandBoundsWithMesh(result.world_bounds, result.has_world_bounds,
                         *renderer->mesh, scene_instance.getWorldMatrix(entity_id));
  }

  result.success = result.mesh_primitive_count > 0;
  if (!result.success) {
    result.error_message = "No mesh primitives imported";
  }
  return result;
}

}  // namespace

GltfSceneImporter::ImportResult GltfSceneImporter::importIntoScene(
    AssetManager* asset_manager, const eastl::string& virtual_path,
    SceneInstance& scene_instance) {
  ImportResult result{};
  if (asset_manager == nullptr) {
    result.error_message = "AssetManager is null";
    return result;
  }

  GltfImportDocument document{};
  if (!asset_manager->openGltfImportDocument(virtual_path, document)) {
    result.error_message = "Failed to open glTF document";
    return result;
  }

  scene_instance.clear();
  scene_instance.setSourcePath(virtual_path);

  result = importGltfDocument(asset_manager, document, scene_instance,
                              k_invalid_entity_id);
  asset_manager->closeGltfImportDocument(document);

  if (result.has_world_bounds) {
    scene_instance.setWorldBounds(result.world_bounds);
  }

  LOG_INFO("[GltfSceneImporter] imported {} primitives from {} (bounds={})",
           result.mesh_primitive_count, virtual_path.c_str(),
           result.has_world_bounds ? "yes" : "no");

  return result;
}

GltfSceneImporter::ImportResult GltfSceneImporter::importUnderEntity(
    AssetManager* asset_manager, const eastl::string& mesh_or_gltf_path,
    SceneInstance& scene_instance, EntityId parent_entity_id) {
  ImportResult result{};
  if (asset_manager == nullptr) {
    result.error_message = "AssetManager is null";
    return result;
  }
  if (!isValid(parent_entity_id)) {
    result.error_message = "Parent entity is invalid";
    return result;
  }

  eastl::string gltf_virtual_path;
  if (!asset_manager->resolveGltfSourcePath(mesh_or_gltf_path, gltf_virtual_path)) {
    result.error_message = "Failed to resolve glTF source path";
    return result;
  }

  GltfImportDocument document{};
  if (!asset_manager->openGltfImportDocument(gltf_virtual_path, document)) {
    result.error_message = "Failed to open glTF document";
    return result;
  }

  result = importUnderOpenDocument(asset_manager, document, scene_instance,
                                   parent_entity_id);
  asset_manager->closeGltfImportDocument(document);

  LOG_INFO(
      "[GltfSceneImporter] imported {} primitives under entity from {} (bounds={})",
      result.mesh_primitive_count, mesh_or_gltf_path.c_str(),
      result.has_world_bounds ? "yes" : "no");

  return result;
}

GltfSceneImporter::ImportResult GltfSceneImporter::importUnderOpenDocument(
    AssetManager* asset_manager, GltfImportDocument& document,
    SceneInstance& scene_instance, EntityId parent_entity_id) {
  ImportResult result{};
  if (asset_manager == nullptr) {
    result.error_message = "AssetManager is null";
    return result;
  }
  if (!isValid(parent_entity_id)) {
    result.error_message = "Parent entity is invalid";
    return result;
  }
  if (document.data == nullptr) {
    result.error_message = "glTF document is not open";
    return result;
  }

  result = importGltfDocument(asset_manager, document, scene_instance,
                              parent_entity_id);

  if (result.has_world_bounds) {
    if (scene_instance.hasWorldBounds()) {
      AABB merged = scene_instance.getWorldBounds();
      merged.expandToInclude(result.world_bounds.min);
      merged.expandToInclude(result.world_bounds.max);
      scene_instance.setWorldBounds(merged);
    } else {
      scene_instance.setWorldBounds(result.world_bounds);
    }
  }

  return result;
}

void GltfSceneImporter::attachEntityMeshes(AssetManager* asset_manager,
                                           SceneInstance& instance,
                                           const Scene& scene) {
  if (asset_manager == nullptr) {
    return;
  }

  eastl::unordered_map<eastl::string, GltfImportDocument> open_documents;

  for (const SceneEntityDefinition& definition : scene.getEntities()) {
    if (definition.mesh_virtual_path.empty()) {
      continue;
    }

    const EntityId entity_id = instance.findEntityByName(definition.name);
    if (!isValid(entity_id)) {
      LOG_WARN("[GltfSceneImporter] mesh entity '{}' not found in scene '{}'",
               definition.name.c_str(), instance.getSourcePath().c_str());
      continue;
    }

    eastl::string mesh_ref = definition.mesh_virtual_path;
    if (isValidGuidFormat(mesh_ref) &&
        g_runtime_global_context.m_asset_registry) {
      const eastl::string path = asset_manager->resolveGuidPath(
          mesh_ref, *g_runtime_global_context.m_asset_registry);
      if (!path.empty()) {
        mesh_ref = path;
      }
    }

    eastl::string gltf_virtual_path;
    if (!asset_manager->resolveGltfSourcePath(mesh_ref, gltf_virtual_path)) {
      LOG_ERROR("[GltfSceneImporter] failed to resolve glTF source for '{}'",
                mesh_ref.c_str());
      continue;
    }

    auto cached = open_documents.find(gltf_virtual_path);
    if (cached == open_documents.end()) {
      GltfImportDocument document{};
      if (!asset_manager->openGltfImportDocument(gltf_virtual_path, document)) {
        LOG_ERROR("[GltfSceneImporter] failed to open glTF '{}' for entity '{}'",
                  gltf_virtual_path.c_str(), definition.name.c_str());
        continue;
      }
      cached = open_documents.emplace(gltf_virtual_path, document).first;
    }

    const ImportResult import_result = importUnderOpenDocument(
        asset_manager, cached->second, instance, entity_id);
    if (!import_result.success) {
      LOG_ERROR("[GltfSceneImporter] failed to import mesh '{}' for entity '{}': {}",
                mesh_ref.c_str(), definition.name.c_str(),
                import_result.error_message.c_str());
      continue;
    }

    LOG_INFO("[GltfSceneImporter] attached {} mesh primitives to entity '{}' in '{}'",
             import_result.mesh_primitive_count, definition.name.c_str(),
             instance.getSourcePath().c_str());
  }

  for (auto& entry : open_documents) {
    asset_manager->closeGltfImportDocument(entry.second);
  }
}

}  // namespace Blunder
