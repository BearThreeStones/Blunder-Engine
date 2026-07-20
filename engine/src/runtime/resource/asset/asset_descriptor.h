#pragma once

#include "EASTL/string.h"

namespace Blunder {

struct MeshImportSettings {
  bool materials{true};
  bool animations{true};
  float scale{1.0f};
};

struct TextureImportSettings {
  bool srgb{true};
  bool generate_mips{false};
};

struct MeshAssetDescriptor {
  eastl::string guid;
  eastl::string source;
  eastl::string archived_source;
  MeshImportSettings import{};
};

struct TextureAssetDescriptor {
  eastl::string guid;
  eastl::string source;
  eastl::string archived_source;
  TextureImportSettings import{};
};

}  // namespace Blunder
