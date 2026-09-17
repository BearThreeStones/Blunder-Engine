#include "runtime/resource/asset_cook/meshlet_builder.h"

#include <cstddef>

#include "meshoptimizer.h"

namespace Blunder {

MeshletPayload buildStaticMeshlets(const eastl::vector<MeshVertex>& vertices,
                                   const eastl::vector<uint32_t>& indices) {
  MeshletPayload payload{};
  if (vertices.empty() || indices.size() < 3u || (indices.size() % 3u) != 0u) {
    return payload;
  }

  const size_t max_meshlets = meshopt_buildMeshletsBound(
      indices.size(), k_meshlet_max_vertices, k_meshlet_max_triangles);
  eastl::vector<meshopt_Meshlet> local(max_meshlets);
  eastl::vector<unsigned int> meshlet_vertices(max_meshlets *
                                               k_meshlet_max_vertices);
  eastl::vector<unsigned char> meshlet_triangles(
      max_meshlets * k_meshlet_max_triangles * 3u);

  const size_t meshlet_count = meshopt_buildMeshlets(
      local.data(), meshlet_vertices.data(), meshlet_triangles.data(),
      indices.data(), indices.size(), &vertices.front().position.x,
      vertices.size(), sizeof(MeshVertex), k_meshlet_max_vertices,
      k_meshlet_max_triangles, 0.0f);
  if (meshlet_count == 0u) {
    return payload;
  }

  const meshopt_Meshlet& last = local[meshlet_count - 1u];
  meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
  meshlet_triangles.resize(last.triangle_offset + last.triangle_count * 3u);

  payload.meshlets.resize(meshlet_count);
  payload.vertices.assign(meshlet_vertices.begin(), meshlet_vertices.end());
  payload.triangles.assign(meshlet_triangles.begin(), meshlet_triangles.end());

  for (size_t i = 0; i < meshlet_count; ++i) {
    const meshopt_Meshlet& src = local[i];
    const meshopt_Bounds bounds = meshopt_computeMeshletBounds(
        meshlet_vertices.data() + src.vertex_offset,
        meshlet_triangles.data() + src.triangle_offset, src.triangle_count,
        &vertices.front().position.x, vertices.size(), sizeof(MeshVertex));

    MeshletRecord& dst = payload.meshlets[i];
    dst.vertex_offset = src.vertex_offset;
    dst.triangle_offset = src.triangle_offset;
    dst.vertex_count = static_cast<uint8_t>(src.vertex_count);
    dst.triangle_count = static_cast<uint8_t>(src.triangle_count);
    dst.center[0] = bounds.center[0];
    dst.center[1] = bounds.center[1];
    dst.center[2] = bounds.center[2];
    dst.radius = bounds.radius;
    dst.cone_axis[0] = bounds.cone_axis_s8[0];
    dst.cone_axis[1] = bounds.cone_axis_s8[1];
    dst.cone_axis[2] = bounds.cone_axis_s8[2];
    dst.cone_cutoff = bounds.cone_cutoff_s8;
  }

  return payload;
}

}  // namespace Blunder
