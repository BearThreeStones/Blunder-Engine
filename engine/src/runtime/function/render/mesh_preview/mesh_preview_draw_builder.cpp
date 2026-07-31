#include "runtime/function/render/mesh_preview/mesh_preview_draw_builder.h"

#include <cgltf.h>

#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_manager/asset_manager_gltf.h"

namespace Blunder {

namespace {

const cgltf_skin* findSkinForMeshIndex(const cgltf_data& data, size_t mesh_index) {
  if (mesh_index >= static_cast<size_t>(data.meshes_count)) {
    return nullptr;
  }
  const cgltf_mesh& target_mesh = data.meshes[mesh_index];
  for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
    const cgltf_node& node = data.nodes[node_index];
    if (node.mesh == &target_mesh && node.skin != nullptr) {
      return node.skin;
    }
  }
  return nullptr;
}

}  // namespace

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

  if (loaded->isFromCookedFinal()) {
    MeshPreviewSubmeshDraw draw{};
    draw.mesh = loaded;
    draw.material = loaded->getMaterialAsset();
    draws.push_back(eastl::move(draw));
    return draws;
  }

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
  for (cgltf_size mesh_index = 0; mesh_index < data->meshes_count; ++mesh_index) {
    const cgltf_mesh& mesh = data->meshes[mesh_index];
    const cgltf_skin* skin = findSkinForMeshIndex(*data, static_cast<size_t>(mesh_index));
    for (cgltf_size primitive_index = 0;
         primitive_index < mesh.primitives_count; ++primitive_index) {
      const eastl::shared_ptr<MeshAsset> primitive_mesh =
          asset_manager.loadMeshPrimitive(
              data, static_cast<size_t>(mesh_index),
              static_cast<size_t>(primitive_index), document.absolute,
              document.canonical_key, skin);
      if (!primitive_mesh) {
        continue;
      }
      MeshPreviewSubmeshDraw draw{};
      draw.mesh = primitive_mesh;
      draw.material = primitive_mesh->getMaterialAsset();
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
