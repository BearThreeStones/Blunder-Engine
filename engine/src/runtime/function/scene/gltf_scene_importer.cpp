#include "runtime/function/scene/gltf_scene_importer.h"

#include <cgltf.h>

#include <cstring>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_manager/asset_manager_gltf.h"

namespace Blunder {

namespace {

glm::mat4 matrixFromCgltfNode(const cgltf_node* node) {
  cgltf_float world_matrix_cgltf[16];
  cgltf_node_transform_world(const_cast<cgltf_node*>(node), world_matrix_cgltf);
  glm::mat4 result(1.0f);
  std::memcpy(glm::value_ptr(result), world_matrix_cgltf, sizeof(cgltf_float) * 16);
  return similarityGltfToEngine(result);
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

  cgltf_data* data = document.data;
  const std::filesystem::path& absolute = document.absolute;
  const eastl::string& gltf_key = document.canonical_key;

  scene_instance.clear();
  scene_instance.setSourcePath(virtual_path);

  const auto visit_node = [&](const auto& visit_self, cgltf_node* node) -> void {
    if (node == nullptr) {
      return;
    }

    const glm::mat4 world_matrix = matrixFromCgltfNode(node);

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
        renderer.world_matrix = world_matrix;
        if (renderer.material) {
          renderer.alpha_mode = renderer.material->getAlphaMode();
          renderer.alpha_cutoff = renderer.material->getAlphaCutoff();
          renderer.double_sided = renderer.material->isDoubleSided();
        }

        char entity_name[128];
        std::snprintf(entity_name, sizeof(entity_name), "mesh%zu_prim%zu",
                      mesh_index, static_cast<size_t>(primitive_index));
        const EntityId entity_id = scene_instance.createEntity(
            entity_name, Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f));
        scene_instance.setMeshRenderer(entity_id, eastl::move(renderer));

        expandBoundsWithMesh(result.world_bounds, result.has_world_bounds,
                             *mesh_asset, world_matrix);
        ++result.mesh_primitive_count;
      }
    }

    for (cgltf_size child_index = 0; child_index < node->children_count;
         ++child_index) {
      visit_self(visit_self, node->children[child_index]);
    }
  };

  if (data->scene != nullptr && data->scene->nodes_count > 0) {
    for (cgltf_size root_index = 0; root_index < data->scene->nodes_count;
         ++root_index) {
      visit_node(visit_node, data->scene->nodes[root_index]);
    }
  } else {
    for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
      cgltf_node* node = &data->nodes[node_index];
      if (node->parent == nullptr) {
        visit_node(visit_node, node);
      }
    }
  }

  asset_manager->closeGltfImportDocument(document);

  if (result.has_world_bounds) {
    scene_instance.setWorldBounds(result.world_bounds);
  }

  result.success = result.mesh_primitive_count > 0;
  if (!result.success) {
    result.error_message = "No mesh primitives imported";
  }

  LOG_INFO("[GltfSceneImporter] imported {} primitives from {} (bounds={})",
           result.mesh_primitive_count, virtual_path.c_str(),
           result.has_world_bounds ? "yes" : "no");

  return result;
}

}  // namespace Blunder
