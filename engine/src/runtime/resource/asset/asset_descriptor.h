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

/// Timed logical event for method-track dispatch (Phase 5).
struct AnimationMethodKey {
  eastl::string name;
  float time{0.0f};
  eastl::vector<float> args;
};

struct AnimationClipData {
  static constexpr int kVersion = 1;
  eastl::string name;
  float duration{0.0f};
  eastl::vector<AnimationTrack> tracks;
  eastl::vector<AnimationMethodKey> method_keys;
};

struct AnimationClipAssetDescriptor {
  eastl::string guid;
  eastl::string source;
  eastl::string archived_source;
};

/// Reusable AnimationTree topology body (Intermediate / Asset).
struct AnimationTreeTopologyData {
  static constexpr int kVersion = 1;

  struct BlendSpace1DPointDef {
    eastl::string clip_name;
    float scalar{0.0f};
  };
  struct BlendSpace1DDef {
    eastl::string node_name;
    float scalar{0.0f};
    eastl::vector<BlendSpace1DPointDef> points;
  };
  struct BlendSpace2DPointDef {
    eastl::string clip_name;
    float x{0.0f};
    float y{0.0f};
  };
  struct BlendSpace2DDef {
    eastl::string node_name;
    float x{0.0f};
    float y{0.0f};
    eastl::vector<BlendSpace2DPointDef> points;
  };
  struct StateDef {
    eastl::string name;
    /// "clip" | "blendSpace1D" | "blendSpace2D"
    eastl::string kind{"clip"};
    eastl::string clip_name;
    eastl::string blend_space_node;
  };

  eastl::string base_blend_space_node;
  eastl::string base_blend_space_2d_node;
  eastl::string add2_clip;
  eastl::string oneshot_clip;
  eastl::vector<BlendSpace1DDef> blend_spaces_1d;
  eastl::vector<BlendSpace2DDef> blend_spaces_2d;
  eastl::vector<StateDef> states;
};

struct AnimationTreeAssetDescriptor {
  eastl::string guid;
  eastl::string source;
  eastl::string archived_source;
};

/// Small scene-instance allowlist over Asset base (Phase 5 D).
struct AnimationTreeInstanceOverrides {
  struct ScalarOverride {
    eastl::string node_name;
    float value{0.0f};
  };
  struct Param2DOverride {
    eastl::string node_name;
    float x{0.0f};
    float y{0.0f};
  };

  bool has_active{false};
  bool active{false};
  bool has_add2_weight{false};
  float add2_weight{0.0f};
  eastl::string current_state;
  eastl::vector<ScalarOverride> blend_space_scalars;
  eastl::vector<Param2DOverride> blend_space_2d_params;
};

}  // namespace Blunder
