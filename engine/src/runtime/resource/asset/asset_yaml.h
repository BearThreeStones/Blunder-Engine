#pragma once

#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AssetYaml final {
 public:
  static bool parseMeshDescriptor(const eastl::string& yaml_text,
                                  MeshAssetDescriptor& out_descriptor);
  static bool parseTextureDescriptor(const eastl::string& yaml_text,
                                     TextureAssetDescriptor& out_descriptor);

  static eastl::string serializeMeshDescriptor(
      const MeshAssetDescriptor& descriptor);
  static eastl::string serializeTextureDescriptor(
      const TextureAssetDescriptor& descriptor);

  /// Reads the `source` field from a mesh or texture YAML descriptor.
  static bool parseSourceField(const eastl::string& yaml_text,
                               eastl::string& out_source);
};

}  // namespace Blunder
