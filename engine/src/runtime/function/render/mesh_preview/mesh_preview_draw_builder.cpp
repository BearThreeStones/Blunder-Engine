#include "runtime/function/render/mesh_preview/mesh_preview_draw_builder.h"

#include <cstring>

#include <cgltf.h>
#include <glm/gtc/type_ptr.hpp>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_manager/asset_manager_gltf.h"

namespace Blunder {

eastl::vector<MeshPreviewSubmeshDraw> collectMeshPreviewSubmeshes(
    AssetManager& asset_manager, const eastl::string& mesh_virtual_path) {
  eastl::vector<MeshPreviewSubmeshDraw> draws;
  if (mesh_virtual_path.empty()) {
    return draws;
  }

  const eastl::shared_ptr<MeshAsset> loaded =
      asset_manager.loadMesh(mesh_virtual_path);
  if (!loaded) {
    return draws;
  }

  // Expand Intermediate glTF node×primitives. Cooked Final alone is often a
  // single-primitive fallback and previously ignored node transforms.
  eastl::string gltf_source;
  if (!asset_manager.resolveGltfSourcePath(mesh_virtual_path, gltf_source)) {
    gltf_source = mesh_virtual_path;
  }

  GltfImportDocument document{};
  if (!asset_manager.openGltfImportDocument(gltf_source, document) ||
      document.data == nullptr) {
    MeshPreviewSubmeshDraw draw{};
    draw.mesh = loaded;
    draw.material = loaded->getMaterialAsset();
    draws.push_back(eastl::move(draw));
    return draws;
  }

  cgltf_data* data = document.data;
  for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
    const cgltf_node& node = data->nodes[node_index];
    if (node.mesh == nullptr) {
      continue;
    }

    const size_t mesh_index =
        static_cast<size_t>(node.mesh - data->meshes);
    if (mesh_index >= static_cast<size_t>(data->meshes_count)) {
      continue;
    }

    cgltf_float world[16];
    cgltf_node_transform_world(&node, world);
    const glm::mat4 model = similarityGltfToEngine(glm::make_mat4(world));

    const cgltf_mesh& mesh = data->meshes[mesh_index];
    const cgltf_skin* skin = node.skin;
    for (cgltf_size primitive_index = 0;
         primitive_index < mesh.primitives_count; ++primitive_index) {
      const eastl::shared_ptr<MeshAsset> primitive_mesh =
          asset_manager.loadMeshPrimitive(
              data, mesh_index, static_cast<size_t>(primitive_index),
              document.absolute, document.canonical_key, skin);
      if (!primitive_mesh) {
        continue;
      }
      MeshPreviewSubmeshDraw draw{};
      draw.mesh = primitive_mesh;
      draw.material = primitive_mesh->getMaterialAsset();
      draw.model = model;
      draws.push_back(eastl::move(draw));
    }
  }

  asset_manager.closeGltfImportDocument(document);

  if (draws.empty()) {
    MeshPreviewSubmeshDraw draw{};
    draw.mesh = loaded;
    draw.material = loaded->getMaterialAsset();
    draws.push_back(eastl::move(draw));
  }

  return draws;
}

}  // namespace Blunder
