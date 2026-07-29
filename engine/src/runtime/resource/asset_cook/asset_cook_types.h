#pragma once

#include <cstdint>

namespace Blunder {

inline constexpr char kMeshCookMagic[4] = {'B', 'L', 'M', 'S'};
inline constexpr uint32_t kMeshCookVersionLegacy = 1;
inline constexpr uint32_t kMeshCookVersion = 2;

inline constexpr uint32_t kMeshCookFlag_HasSkin = 1u;

#pragma pack(push, 1)
struct MeshCookHeader {
  char magic[4];
  uint32_t version{0};
  uint32_t vertex_count{0};
  uint32_t index_count{0};
  uint32_t vertex_stride{0};
  uint32_t flags{0};  // present when version >= kMeshCookVersion
};
#pragma pack(pop)

inline constexpr size_t kMeshCookHeaderV1Size =
    sizeof(MeshCookHeader) - sizeof(uint32_t);

struct CookedAssetMeta {
  uint64_t source_mtime{0};
  uint64_t descriptor_mtime{0};
};

}  // namespace Blunder
