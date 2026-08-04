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

  static bool parseAnimationClipDescriptor(
      const eastl::string& yaml_text,
      AnimationClipAssetDescriptor& out_descriptor);
  static eastl::string serializeAnimationClipDescriptor(
      const AnimationClipAssetDescriptor& descriptor);

  static bool parseAnimationClipData(const eastl::string& yaml_text,
                                     AnimationClipData& out_data);
  static eastl::string serializeAnimationClipData(
      const AnimationClipData& data);

  static bool parseAnimationTreeAssetDescriptor(
      const eastl::string& yaml_text,
      AnimationTreeAssetDescriptor& out_descriptor);
  static eastl::string serializeAnimationTreeAssetDescriptor(
      const AnimationTreeAssetDescriptor& descriptor);

  static bool parseAnimationTreeTopologyData(
      const eastl::string& yaml_text, AnimationTreeTopologyData& out_data);
  static eastl::string serializeAnimationTreeTopologyData(
      const AnimationTreeTopologyData& data);

  /// Reads the `source` field from a mesh or texture YAML descriptor.
  static bool parseSourceField(const eastl::string& yaml_text,
                               eastl::string& out_source);
};

}  // namespace Blunder
