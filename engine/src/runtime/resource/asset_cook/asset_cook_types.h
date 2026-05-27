#pragma once

#include <cstdint>

namespace Blunder {

inline constexpr char kMeshCookMagic[4] = {'B', 'L', 'M', 'S'};
inline constexpr uint32_t kMeshCookVersion = 1;

#pragma pack(push, 1)
struct MeshCookHeader {
  char magic[4];
  uint32_t version{0};
  uint32_t vertex_count{0};
  uint32_t index_count{0};
  uint32_t vertex_stride{0};
};
#pragma pack(pop)

struct CookedAssetMeta {
  uint64_t source_mtime{0};
  uint64_t descriptor_mtime{0};
};

}  // namespace Blunder
