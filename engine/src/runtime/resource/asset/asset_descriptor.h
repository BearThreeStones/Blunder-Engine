#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

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
  /// Optional explicit Texture Asset GUID references (Mesh→Texture graph edges).
  eastl::vector<eastl::string> texture_guids;
  /// Resources Intermediate glTF/GLB bodies paired with this mesh for Reimport.
  eastl::vector<eastl::string> companion_animation_sources;
  MeshImportSettings import{};
};

struct TextureAssetDescriptor {
  eastl::string guid;
  eastl::string source;
  eastl::string archived_source;
  TextureImportSettings import{};
};

enum class AnimationInterpolation {
  Constant,
  Linear,
};

enum class AnimationChannel {
  Translation,
  Rotation,
  Scale,
};

struct AnimationKeyframe {
  float time{0.0f};
  /// Translation/scale: 3 floats; rotation: 4 floats (quaternion xyzw).
  eastl::vector<float> value;
};

struct AnimationTrack {
  eastl::string bone;
  AnimationChannel channel{AnimationChannel::Translation};
  AnimationInterpolation interpolation{AnimationInterpolation::Constant};
  eastl::vector<AnimationKeyframe> keys;
};

struct AnimationClipData {
  static constexpr int kVersion = 1;
  eastl::string name;
  float duration{0.0f};
  eastl::vector<AnimationTrack> tracks;
};

struct AnimationClipAssetDescriptor {
  eastl::string guid;
  eastl::string source;
  eastl::string archived_source;
};

}  // namespace Blunder
