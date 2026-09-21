#pragma once

#include <cstdint>

namespace Blunder {

inline constexpr char kMeshCookMagic[4] = {'B', 'L', 'M', 'S'};
inline constexpr uint32_t kMeshCookVersionLegacy = 1;
inline constexpr uint32_t kMeshCookVersionSkin = 2;
inline constexpr uint32_t kMeshCookVersionMeshlets = 3;
inline constexpr uint32_t kMeshCookVersion = 4;

static_assert(kMeshCookVersionMeshlets < kMeshCookVersion,
              "COLOR_0 cook format is newer than meshlet-only cook");

inline constexpr uint32_t kMeshCookFlag_HasSkin = 1u;
inline constexpr uint32_t kMeshCookFlag_HasMeshlets = 2u;

#pragma pack(push, 1)
struct MeshCookHeader {
  char magic[4];
  uint32_t version{0};
  uint32_t vertex_count{0};
  uint32_t index_count{0};
  uint32_t vertex_stride{0};
  uint32_t flags{0};  // present when version >= kMeshCookVersionSkin
};
#pragma pack(pop)

inline constexpr size_t kMeshCookHeaderV1Size =
    sizeof(MeshCookHeader) - sizeof(uint32_t);

struct CookedAssetMeta {
  uint64_t source_mtime{0};
  uint64_t descriptor_mtime{0};
  // Cook format version the Final was written with. Mesh cook writes
  // kMeshCookVersion; texture cook leaves it 0. A meta file without the
  // `cook_format` key reads as 0. Version 4 packs glTF COLOR_0 on MeshVertex.
  uint32_t cook_format{0};
};

}  // namespace Blunder
