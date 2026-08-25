#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "EASTL/string.h"
#include "EASTL/utility.h"
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

struct OptionalOverrideFloat {
  bool present{false};
  float value{0.0f};

  bool operator==(const OptionalOverrideFloat& other) const {
    return present == other.present && (!present || value == other.value);
  }
};

struct OptionalOverrideBool {
  bool present{false};
  bool value{false};

  bool operator==(const OptionalOverrideBool& other) const {
    return present == other.present && (!present || value == other.value);
  }
};

struct OptionalOverrideVec3 {
  bool present{false};
  glm::vec3 value{0.0f};

  bool operator==(const OptionalOverrideVec3& other) const {
    return present == other.present && (!present || value == other.value);
  }
};

struct OptionalOverrideVec4 {
  bool present{false};
  glm::vec4 value{1.0f};

  bool operator==(const OptionalOverrideVec4& other) const {
    return present == other.present && (!present || value == other.value);
  }
};

/// present + empty guid = suppress Import texture. Absent = keep Import.
struct OptionalOverrideSlot {
  bool present{false};
  eastl::string guid;

  bool operator==(const OptionalOverrideSlot& other) const {
    return present == other.present && (!present || guid == other.guid);
  }
};

struct MeshMaterialOverride {
  OptionalOverrideBool unlit;
  OptionalOverrideVec4 base_color;
  OptionalOverrideFloat metallic;
  OptionalOverrideFloat roughness;
  OptionalOverrideVec3 ambient;
  OptionalOverrideVec3 diffuse;
  OptionalOverrideVec3 specular;
  OptionalOverrideFloat shininess;
  OptionalOverrideSlot base_color_texture;
  OptionalOverrideSlot metallic_roughness_texture;
  OptionalOverrideSlot normal_texture;
  OptionalOverrideSlot occlusion_texture;

  bool empty() const {
    return !unlit.present && !base_color.present && !metallic.present &&
           !roughness.present && !ambient.present && !diffuse.present &&
           !specular.present && !shininess.present &&
           !base_color_texture.present &&
           !metallic_roughness_texture.present && !normal_texture.present &&
           !occlusion_texture.present;
  }

  bool operator==(const MeshMaterialOverride& other) const {
    return unlit == other.unlit && base_color == other.base_color &&
           metallic == other.metallic && roughness == other.roughness &&
           ambient == other.ambient && diffuse == other.diffuse &&
           specular == other.specular && shininess == other.shininess &&
           base_color_texture == other.base_color_texture &&
           metallic_roughness_texture == other.metallic_roughness_texture &&
           normal_texture == other.normal_texture &&
           occlusion_texture == other.occlusion_texture;
  }
};

struct MeshAssetDescriptor;

/// `texture_guids` = Import-discovered ∪ non-empty override slot GUIDs.
void rebuildMeshTextureGuids(MeshAssetDescriptor& descriptor);

struct MeshAssetDescriptor {
  eastl::string guid;
  eastl::string source;
  eastl::string archived_source;
  /// Graph edges: Import-discovered ∪ non-empty override slots.
  eastl::vector<eastl::string> texture_guids;
  /// Import-discovered Texture GUIDs before override union. Empty on legacy
  /// descriptors: treat `texture_guids` as Import-only until first override write.
  eastl::vector<eastl::string> import_texture_guids;
  /// Deprecated packaging list (ADR 0028). Ignored on Import; cleared by migration.
  eastl::vector<eastl::string> companion_animation_sources;
  MeshImportSettings import{};
  MeshMaterialOverride material_override{};
  /// Unknown root YAML keys preserved on round-trip (Dump of each value).
  eastl::vector<eastl::pair<eastl::string, eastl::string>> unknown_root_fields;
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
  static constexpr int kVersion = 2;

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
  struct TreeParamDef {
    eastl::string name;
    /// "bool" | "float"
    eastl::string kind{"float"};
    bool bool_default{false};
    float float_default{0.0f};
  };
  struct TransitionDef {
    eastl::string from_state;
    eastl::string to_state;
    /// "treeParam" | "blendSpace1DScalar" | "blendSpace2DX" | "blendSpace2DY" | "add2Weight"
    eastl::string source{"treeParam"};
    eastl::string param_name;
    bool is_bool_predicate{false};
    /// "eq" | "ne" | "lt" | "le" | "gt" | "ge"
    eastl::string op{"eq"};
    float float_operand{0.0f};
    bool bool_operand{true};
    int priority{0};
  };
  struct CanvasLayoutNodeDef {
    eastl::string node_id;
    float x{0.0f};
    float y{0.0f};
  };

  eastl::string base_blend_space_node;
  eastl::string base_blend_space_2d_node;
  eastl::string add2_clip;
  eastl::string oneshot_clip;
  eastl::vector<BlendSpace1DDef> blend_spaces_1d;
  eastl::vector<BlendSpace2DDef> blend_spaces_2d;
  eastl::vector<StateDef> states;
  eastl::vector<TreeParamDef> tree_params;
  eastl::vector<TransitionDef> transitions;
  eastl::vector<CanvasLayoutNodeDef> canvas_layout;
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
