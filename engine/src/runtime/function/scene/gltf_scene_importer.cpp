#include <chrono>
#include <cmath>

#include "runtime/function/scene/gltf_scene_importer.h"

#include <cgltf.h>

#include <cstdio>
#include <cstring>
#include <filesystem>

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
#include "runtime/function/render/mesh_loader.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/gltf_node_extras.h"
#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
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

bool entityHoldsUniqueComponents(const SceneInstance& scene, EntityId id) {
  return scene.getLight(id) != nullptr || scene.getCamera(id) != nullptr ||
         scene.getFog(id) != nullptr;
}

EntityId findReusableNamedEntity(SceneInstance& scene, EntityId attach_root,
                                 EntityId parent_id, const eastl::string& name,
                                 const eastl::unordered_set<EntityId>& used) {
  EntityId found = findUnusedDirectChild(scene, parent_id, name, used);
  if (isValid(found) && !entityHoldsUniqueComponents(scene, found)) {
    return found;
  }
  found = findUnusedDescendant(scene, attach_root, name, used);
  if (isValid(found) && entityHoldsUniqueComponents(scene, found)) {
    return k_invalid_entity_id;
  }
  return found;
}

bool ancestorChainHasUniformScale(const SceneInstance& scene, EntityId start_parent,
                                  float scale) {
  EntityId current = start_parent;
  while (isValid(current)) {
    Vec3 position{};
    Quat rotation = glm::identity<Quat>();
    Vec3 parent_scale(1.0f);
    if (!scene.getTransform(current, position, rotation, parent_scale)) {
      break;
    }
    if (isUniformScale(parent_scale) &&
        std::fabs(parent_scale.x - scale) <= 1e-4f) {
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

void maybeAbsorbNodeScaleIntoAncestor(const SceneInstance& scene,
                                      EntityId parent_entity_id, Vec3& local_scale) {
  if (!isGltfCentimeterUniformScale(local_scale)) {
    return;
  }
  // Drop nodesQ0S 0.008 under an attach Unique so mesh verts stay centimetre
  // (matching identity GPU draws and courtyard Uniques). Also drop it when an
  // ancestor already carries the same uniform scale (no double 0.008).
  if (isValid(parent_entity_id) ||
      ancestorChainHasUniformScale(scene, parent_entity_id, local_scale.x)) {
    local_scale = Vec3(1.0f);
  }
}

void convertMeterSpaceUniquesBesideGltfScale(SceneInstance& scene,
                                             EntityId attach_root) {
  if (!isValid(attach_root)) {
    return;
  }
  Vec3 root_position{};
  Quat root_rotation = glm::identity<Quat>();
  Vec3 root_scale(1.0f);
  if (!scene.getTransform(attach_root, root_position, root_rotation, root_scale) ||
      !isGltfCentimeterUniformScale(root_scale)) {
    return;
  }

  const auto convert_if_meters = [&](EntityId id) {
    Entity* entity = scene.getEntity(id);
    if (entity == nullptr || entity->getParentId() != attach_root) {
      return;
    }
    const Vec3 local = entity->getPosition();
    if (!looksLikeMeterSpaceTranslation(local)) {
      return;
    }
    scene.setTransform(id, meterTranslationToGltfLocal(local, root_scale.x),
                       entity->getRotation(), entity->getScale());
  };

  scene.forEachLight(
      [&](EntityId id, const LightComponent&) { convert_if_meters(id); });
  scene.forEachCamera(
      [&](EntityId id, const CameraComponent&) { convert_if_meters(id); });
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
    // Grill locked: omit COL-* (no MeshRenderer, no inactive placeholder entity).
    if (gltfNodeNameStartsWith(node, "COL-")) {
      return;
    }
    eastl::string instance_asset_id;
    // Runtime attach/import is not the flatten bake: ignore instance_asset_id.
    if (gltfNodeInstanceAssetId(node, instance_asset_id)) {
      return;
    }
    Vec3 local_position{};
    Quat local_rotation = glm::identity<Quat>();
    Vec3 local_scale(1.0f);
    decomposeCgltfNodeLocal(node, local_position, local_rotation, local_scale);
    maybeAbsorbNodeScaleIntoAncestor(scene_instance, parent_entity_id,
                                     local_scale);

    EntityId node_entity_id = findReusableNamedEntity(
        scene_instance, attach_parent_entity, parent_entity_id, node_name,
        reused_ids);
    if (isValid(node_entity_id)) {
      reused_ids.insert(node_entity_id);
      scene_instance.setTransform(node_entity_id, local_position, local_rotation,
                                  local_scale);
    } else {
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

  convertMeterSpaceUniquesBesideGltfScale(scene_instance, attach_parent_entity);
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

namespace {

bool endsWithInsensitive(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  for (size_t i = 0; i < suffix_length; ++i) {
    char a = value[value.size() - suffix_length + i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<char>(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = static_cast<char>(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

bool meshRefLooksLikeAsset(const eastl::string& mesh_ref) {
  return isValidGuidFormat(mesh_ref) ||
         endsWithInsensitive(mesh_ref, ".mesh.yaml") ||
         endsWithInsensitive(mesh_ref, ".mesh.asset");
}

void bindMeshAssetRenderer(SceneInstance& instance, EntityId entity_id,
                           const eastl::shared_ptr<MeshAsset>& mesh) {
  MeshRendererComponent renderer{};
  renderer.mesh = mesh;
  renderer.material = mesh->getMaterialAsset();
  if (renderer.material) {
    renderer.alpha_mode = renderer.material->getAlphaMode();
    renderer.alpha_cutoff = renderer.material->getAlphaCutoff();
    renderer.double_sided = renderer.material->isDoubleSided();
  }
  instance.setMeshRenderer(entity_id, eastl::move(renderer));
}

void bindPendingMeshRenderer(SceneInstance& instance, EntityId entity_id,
                             const eastl::string& key) {
  MeshRendererComponent renderer{};
  renderer.pending_mesh_key = key;
  instance.setMeshRenderer(entity_id, eastl::move(renderer));
}

bool enqueueUniqueMesh(AssetManager* asset_manager, MeshLoader* mesh_loader,
                       const eastl::string& definition_ref,
                       const eastl::string& resolved_ref,
                       eastl::string& out_key) {
  out_key.clear();
  if (asset_manager == nullptr || mesh_loader == nullptr) {
    return false;
  }
  FileSystem* file_system = asset_manager->fileSystem();
  if (file_system == nullptr) {
    return false;
  }

  MeshLoader::Request request{};
  eastl::string virtual_path = resolved_ref.empty() ? definition_ref : resolved_ref;
  if (isValidGuidFormat(definition_ref)) {
    request.key = definition_ref;
    request.guid = definition_ref;
    if (!resolved_ref.empty() && !isValidGuidFormat(resolved_ref)) {
      virtual_path = resolved_ref;
    }
  } else {
    request.key = virtual_path;
  }
  request.virtual_path = virtual_path;

  if (endsWithInsensitive(virtual_path, ".mesh.yaml") ||
      endsWithInsensitive(virtual_path, ".mesh.asset")) {
    const char* relative = virtual_path.c_str();
    if (virtual_path.size() >= 7 &&
        (std::strncmp(relative, "assets/", 7) == 0 ||
         std::strncmp(relative, "Assets/", 7) == 0)) {
      relative += 7;
    }
    request.descriptor_path =
        file_system->resolveAsset(std::filesystem::path(relative));
    eastl::string yaml_text;
    MeshAssetDescriptor descriptor{};
    if (file_system->readText(request.descriptor_path, yaml_text) &&
        AssetYaml::parseMeshDescriptor(yaml_text, descriptor)) {
      if (request.guid.empty()) {
        request.guid = descriptor.guid;
      }
      if (!descriptor.source.empty()) {
        const char* source = descriptor.source.c_str();
        if (descriptor.source.size() >= 10 &&
            (std::strncmp(source, "resources/", 10) == 0 ||
             std::strncmp(source, "Resources/", 10) == 0)) {
          source += 10;
        }
        request.source_path =
            file_system->resolveResource(std::filesystem::path(source));
      }
    }
  }

  if (!request.guid.empty()) {
    request.cooked_path = cookedMeshPath(*file_system, request.guid);
    if (!isValidGuidFormat(request.key)) {
      request.key = request.guid;
    }
  }
  if (request.key.empty()) {
    request.key = definition_ref;
  }
  out_key = request.key;
  mesh_loader->request(request);
  return true;
}

}  // namespace

void GltfSceneImporter::attachEntityMeshes(AssetManager* asset_manager,
                                           SceneInstance& instance,
                                           const Scene& scene,
                                           MeshLoader* mesh_loader) {
  if (asset_manager == nullptr || !instance.instantiateCompleted()) {
    return;
  }

  eastl::unordered_map<eastl::string, GltfImportDocument> open_documents;
  eastl::unordered_map<eastl::string, eastl::shared_ptr<MeshAsset>> mesh_by_ref;
  eastl::unordered_map<eastl::string, eastl::string> stream_key_by_ref;
  size_t mesh_asset_binds = 0;
  size_t gltf_imports = 0;
  size_t stream_enqueues = 0;

  LOG_INFO("[GltfSceneImporter] attachEntityMeshes begin (entities={})",
           scene.getEntities().size());
  const auto attach_begin = std::chrono::steady_clock::now();

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

    if (auto streamed = stream_key_by_ref.find(definition.mesh_virtual_path);
        streamed != stream_key_by_ref.end()) {
      bindPendingMeshRenderer(instance, entity_id, streamed->second);
      ++mesh_asset_binds;
      continue;
    }

    if (auto cached = mesh_by_ref.find(definition.mesh_virtual_path);
        cached != mesh_by_ref.end()) {
      bindMeshAssetRenderer(instance, entity_id, cached->second);
      ++mesh_asset_binds;
      continue;
    }

    eastl::string mesh_ref = definition.mesh_virtual_path;
    AssetRegistry* registry = g_runtime_global_context.m_asset_registry.get();
    if (isValidGuidFormat(mesh_ref) && registry != nullptr) {
      const eastl::string path = asset_manager->resolveGuidPath(mesh_ref, *registry);
      if (!path.empty()) {
        mesh_ref = path;
      }
    }

    // Flattened Scene Assets stamp a Mesh Asset GUID on each instance entity.
    // Re-importing the source glTF graph under every copy walks/ticks the
    // whole document ~N times (SE-world: ~10k) and never leaves AppInit.
    if (meshRefLooksLikeAsset(definition.mesh_virtual_path) ||
        meshRefLooksLikeAsset(mesh_ref)) {
      if (mesh_loader != nullptr) {
        eastl::string stream_key;
        if (enqueueUniqueMesh(asset_manager, mesh_loader,
                              definition.mesh_virtual_path, mesh_ref,
                              stream_key)) {
          stream_key_by_ref[definition.mesh_virtual_path] = stream_key;
          bindPendingMeshRenderer(instance, entity_id, stream_key);
          ++mesh_asset_binds;
          ++stream_enqueues;
          continue;
        }
      }
      eastl::shared_ptr<MeshAsset> mesh;
      if (isValidGuidFormat(definition.mesh_virtual_path) &&
          registry != nullptr) {
        mesh = asset_manager->loadMeshByGuid(definition.mesh_virtual_path, *registry);
      }
      if (!mesh) {
        mesh = asset_manager->loadMesh(mesh_ref);
      }
      if (mesh) {
        mesh_by_ref[definition.mesh_virtual_path] = mesh;
        bindMeshAssetRenderer(instance, entity_id, mesh);
        ++mesh_asset_binds;
        if (mesh_asset_binds % 1000 == 0) {
          LOG_INFO("[GltfSceneImporter] Mesh Asset binds so far: {}",
                   mesh_asset_binds);
        }
        continue;
      }
      LOG_WARN(
          "[GltfSceneImporter] Mesh Asset bind failed for '{}' ({}); "
          "falling back to glTF import",
          definition.name.c_str(), mesh_ref.c_str());
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
    ++gltf_imports;
  }

  for (auto& entry : open_documents) {
    asset_manager->closeGltfImportDocument(entry.second);
  }

  size_t hydrated = 0;
  if (mesh_loader == nullptr) {
    hydrated = asset_manager->tickDeferredGltfMaterials(~0u);
    instance.rebindMeshRendererMaterialsFromMeshes();
  }

  LOG_INFO(
      "[GltfSceneImporter] attached MeshRenderers in '{}' (mesh assets={}, "
      "gltf imports={}, unique={}, streamed={}, deferred_hydrate={}, {:.1f}ms)",
      instance.getSourcePath().c_str(), mesh_asset_binds, gltf_imports,
      mesh_by_ref.size() + stream_key_by_ref.size(), stream_enqueues, hydrated,
      std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - attach_begin)
          .count());
}

}  // namespace Blunder
