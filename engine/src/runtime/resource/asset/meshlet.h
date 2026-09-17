#pragma once

#include <cstdint>

#include "EASTL/vector.h"

namespace Blunder {

inline constexpr uint32_t k_meshlet_max_vertices = 64u;
inline constexpr uint32_t k_meshlet_max_triangles = 124u;

#pragma pack(push, 1)
struct MeshletRecord {
  uint32_t vertex_offset{0};
  uint32_t triangle_offset{0};
  uint8_t vertex_count{0};
  uint8_t triangle_count{0};
  uint8_t pad[2]{0, 0};
  float center[3]{0.0f, 0.0f, 0.0f};
  float radius{0.0f};
  int8_t cone_axis[3]{0, 0, 0};
  int8_t cone_cutoff{0};
};
#pragma pack(pop)

struct MeshletPayload {
  eastl::vector<MeshletRecord> meshlets;
  eastl::vector<uint32_t> vertices;
  eastl::vector<uint8_t> triangles;

  bool empty() const { return meshlets.empty(); }
};

}  // namespace Blunder
