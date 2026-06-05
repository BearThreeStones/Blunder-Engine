#include "runtime/function/scene/gltf_scene_importer.h"

#include <cgltf.h>

#include <cstring>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_manager/asset_manager_gltf.h"

namespace Blunder {

namespace {

void decomposeCgltfNodeLocal(const cgltf_node* node, Vec3& out_position,
                             Quat& out_rotation, Vec3& out_scale) {
  cgltf_float local_matrix_cgltf[16];
  cgltf_node_transform_local(const_cast<cgltf_node*>(node), local_matrix_cgltf);
  glm::mat4 local_matrix(1.0f);
  std::memcpy(glm::value_ptr(local_matrix), local_matrix_cgltf,
              sizeof(cgltf_float) * 16);
  local_matrix = similarityGltfToEngine(local_matrix);

  out_position = Vec3(local_matrix[3]);
  const Vec3 basis_x = Vec3(local_matrix[0]);
  const Vec3 basis_y = Vec3(local_matrix[1]);
  const Vec3 basis_z = Vec3(local_matrix[2]);
  out_scale = Vec3(glm::length(basis_x), glm::length(basis_y), glm::length(basis_z));

  constexpr float k_epsilon = 1e-6f;
  const Vec3 inv_scale(
      out_scale.x > k_epsilon ? 1.0f / out_scale.x : 1.0f,
      out_scale.y > k_epsilon ? 1.0f / out_scale.y : 1.0f,
      out_scale.z > k_epsilon ? 1.0f / out_scale.z : 1.0f);
  const Mat3 rotation_matrix(glm::normalize(basis_x * inv_scale.x),
                             glm::normalize(basis_y * inv_scale.y),
                             glm::normalize(basis_z * inv_scale.z));
  out_rotation = glm::normalize(glm::quat_cast(rotation_matrix));
}

void expandBoundsWithMesh(AABB& bounds, bool& has_bounds, const MeshAsset& mesh,
                          const glm::mat4& world_matrix) {
  for (const MeshVertex& vertex : mesh.getVertices()) {
    const glm::vec4 world =
        world_matrix * glm::vec4(vertex.position, 1.0f);
    if (!has_bounds) {
      bounds.min = glm::vec3(world);
      bounds.max = glm::vec3(world);
      has_bounds = true;
    } else {
      bounds.expandToInclude(glm::vec3(world));
    }
  }
}

eastl::string nodeDisplayName(const cgltf_node* node) {
  if (node != nullptr && node->name != nullptr && node->name[0] != '\0') {
    return eastl::string(node->name);
  }
  return eastl::string("node");
}

GltfSceneImporter::ImportResult importGltfDocument(
    AssetManager* asset_manager, GltfImportDocument& document,
    SceneInstance& scene_instance, EntityId attach_parent_entity) {
  GltfSceneImporter::ImportResult result{};
  cgltf_data* data = document.data;
  const std::filesystem::path& absolute = document.absolute;
  const eastl::string& gltf_key = document.canonical_key;

  const auto visit_node = [&](const auto& visit_self, cgltf_node* node,
                              EntityId parent_entity_id) -> void {
    if (node == nullptr) {
      return;
    }

    Vec3 local_position{};
    Quat local_rotation = glm::identity<Quat>();
    Vec3 local_scale(1.0f);
    decomposeCgltfNodeLocal(node, local_position, local_rotation, local_scale);

    const EntityId node_entity_id = scene_instance.createEntity(
        nodeDisplayName(node), local_position, local_rotation, local_scale,
        parent_entity_id);

    if (node->mesh != nullptr) {
      const size_t mesh_index = static_cast<size_t>(node->mesh - data->meshes);
      const cgltf_mesh& mesh = *node->mesh;
      for (cgltf_size primitive_index = 0;
           primitive_index < mesh.primitives_count; ++primitive_index) {
        const eastl::shared_ptr<MeshAsset> mesh_asset =
            asset_manager->loadMeshPrimitive(data, mesh_index,
                                             static_cast<size_t>(primitive_index),
                                             absolute, gltf_key);
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
                      nodeDisplayName(node).c_str(),
                      static_cast<size_t>(primitive_index));
        const EntityId primitive_entity_id = scene_instance.createEntity(
            primitive_name, Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f),
            node_entity_id);
        scene_instance.setMeshRenderer(primitive_entity_id, eastl::move(renderer));
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

  scene_instance.forEachMeshRenderer([&](EntityId entity_id,
                                         const MeshRendererComponent& renderer) {
    if (!renderer.mesh) {
      return;
    }
    expandBoundsWithMesh(result.world_bounds, result.has_world_bounds,
                         *renderer.mesh, scene_instance.getWorldMatrix(entity_id));
  });

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

  result = importGltfDocument(asset_manager, document, scene_instance,
                              parent_entity_id);
  asset_manager->closeGltfImportDocument(document);

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

  LOG_INFO(
      "[GltfSceneImporter] imported {} primitives under entity from {} (bounds={})",
      result.mesh_primitive_count, mesh_or_gltf_path.c_str(),
      result.has_world_bounds ? "yes" : "no");

  return result;
}

}  // namespace Blunder
