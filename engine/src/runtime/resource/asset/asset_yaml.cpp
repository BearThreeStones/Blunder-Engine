#include "runtime/resource/asset/asset_yaml.h"

#include <string>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <yaml-cpp/yaml.h>

#include "runtime/core/base/macro.h"

namespace Blunder {

namespace {

bool readStringField(const YAML::Node& root, const char* key,
                     eastl::string& out_value) {
  const YAML::Node node = root[key];
  if (!node || !node.IsScalar()) {
    return false;
  }
  out_value = node.as<std::string>().c_str();
  return !out_value.empty();
}

void readOptionalStringField(const YAML::Node& root, const char* key,
                             eastl::string& out_value) {
  const YAML::Node node = root[key];
  if (!node || !node.IsScalar()) {
    out_value.clear();
    return;
  }
  out_value = node.as<std::string>().c_str();
}

bool readBoolField(const YAML::Node& root, const char* key, bool default_value,
                   bool& out_value) {
  const YAML::Node node = root[key];
  if (!node || !node.IsScalar()) {
    out_value = default_value;
    return false;
  }
  out_value = node.as<bool>();
  return true;
}

bool readFloatField(const YAML::Node& root, const char* key, float default_value,
                    float& out_value) {
  const YAML::Node node = root[key];
  if (!node || !node.IsScalar()) {
    out_value = default_value;
    return false;
  }
  out_value = node.as<float>();
  return true;
}

YAML::Node loadRoot(const eastl::string& yaml_text) {
  return YAML::Load(yaml_text.c_str());
}

bool isKnownMeshRootKey(const std::string& key) {
  return key == "type" || key == "guid" || key == "source" ||
         key == "archived_source" || key == "texture_guids" ||
         key == "import_texture_guids" ||
         key == "companion_animation_sources" || key == "import" ||
         key == "material_override";
}

void readGuidSequence(const YAML::Node& node,
                      eastl::vector<eastl::string>& out_guids) {
  out_guids.clear();
  if (!node || !node.IsSequence()) {
    return;
  }
  for (const auto& item : node) {
    if (!item || !item.IsScalar()) {
      continue;
    }
    const eastl::string guid = item.as<std::string>().c_str();
    if (!guid.empty()) {
      out_guids.push_back(guid);
    }
  }
}

void writeGuidSequence(YAML::Emitter& emitter, const char* key,
                       const eastl::vector<eastl::string>& guids) {
  if (guids.empty()) {
    return;
  }
  emitter << YAML::Key << key << YAML::Value << YAML::BeginSeq;
  for (const eastl::string& guid : guids) {
    emitter << guid.c_str();
  }
  emitter << YAML::EndSeq;
}

bool readOptionalFloat(const YAML::Node& parent, const char* key,
                       OptionalOverrideFloat& out_value) {
  const YAML::Node node = parent[key];
  if (!node || !node.IsScalar()) {
    out_value = {};
    return false;
  }
  out_value.present = true;
  out_value.value = node.as<float>();
  return true;
}

bool readOptionalBool(const YAML::Node& parent, const char* key,
                      OptionalOverrideBool& out_value) {
  const YAML::Node node = parent[key];
  if (!node || !node.IsScalar()) {
    out_value = {};
    return false;
  }
  out_value.present = true;
  out_value.value = node.as<bool>();
  return true;
}

bool readOptionalVec3(const YAML::Node& parent, const char* key,
                      OptionalOverrideVec3& out_value) {
  const YAML::Node node = parent[key];
  if (!node || !node.IsSequence() || node.size() < 3) {
    out_value = {};
    return false;
  }
  out_value.present = true;
  out_value.value = glm::vec3(node[0].as<float>(), node[1].as<float>(),
                              node[2].as<float>());
  return true;
}

bool readOptionalVec4(const YAML::Node& parent, const char* key,
                      OptionalOverrideVec4& out_value) {
  const YAML::Node node = parent[key];
  if (!node || !node.IsSequence() || node.size() < 3) {
    out_value = {};
    return false;
  }
  out_value.present = true;
  const float alpha = node.size() >= 4 ? node[3].as<float>() : 1.0f;
  out_value.value = glm::vec4(node[0].as<float>(), node[1].as<float>(),
                              node[2].as<float>(), alpha);
  return true;
}

bool readOptionalSlot(const YAML::Node& parent, const char* key,
                      OptionalOverrideSlot& out_value) {
  const YAML::Node node = parent[key];
  if (!node || node.IsNull()) {
    out_value = {};
    return false;
  }
  if (!node.IsScalar()) {
    out_value = {};
    return false;
  }
  out_value.present = true;
  out_value.guid = node.as<std::string>().c_str();
  return true;
}

void parseMaterialOverride(const YAML::Node& node, MeshMaterialOverride& out) {
  out = {};
  if (!node || !node.IsMap()) {
    return;
  }
  readOptionalBool(node, "unlit", out.unlit);
  readOptionalVec4(node, "base_color", out.base_color);
  readOptionalFloat(node, "metallic", out.metallic);
  readOptionalFloat(node, "roughness", out.roughness);
  readOptionalVec3(node, "ambient", out.ambient);
  readOptionalVec3(node, "diffuse", out.diffuse);
  readOptionalVec3(node, "specular", out.specular);
  readOptionalFloat(node, "shininess", out.shininess);
  const YAML::Node textures = node["textures"];
  const YAML::Node slot_parent =
      textures && textures.IsMap() ? textures : node;
  readOptionalSlot(slot_parent, "base_color", out.base_color_texture);
  readOptionalSlot(slot_parent, "metallic_roughness",
                   out.metallic_roughness_texture);
  readOptionalSlot(slot_parent, "normal", out.normal_texture);
  readOptionalSlot(slot_parent, "occlusion", out.occlusion_texture);
}

void emitOptionalFloat(YAML::Emitter& emitter, const char* key,
                       const OptionalOverrideFloat& value) {
  if (value.present) {
    emitter << YAML::Key << key << YAML::Value << value.value;
  }
}

void emitOptionalBool(YAML::Emitter& emitter, const char* key,
                      const OptionalOverrideBool& value) {
  if (value.present) {
    emitter << YAML::Key << key << YAML::Value << value.value;
  }
}

void emitVec3(YAML::Emitter& emitter, const glm::vec3& value) {
  emitter << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z
          << YAML::EndSeq;
}

void emitVec4(YAML::Emitter& emitter, const glm::vec4& value) {
  emitter << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z
          << value.w << YAML::EndSeq;
}

void emitOptionalSlot(YAML::Emitter& emitter, const char* key,
                      const OptionalOverrideSlot& value) {
  if (!value.present) {
    return;
  }
  emitter << YAML::Key << key << YAML::Value << value.guid.c_str();
}

void serializeMaterialOverride(YAML::Emitter& emitter,
                               const MeshMaterialOverride& overlay) {
  if (overlay.empty()) {
    return;
  }
  emitter << YAML::Key << "material_override" << YAML::Value << YAML::BeginMap;
  emitOptionalBool(emitter, "unlit", overlay.unlit);
  if (overlay.base_color.present) {
    emitter << YAML::Key << "base_color" << YAML::Value;
    emitVec4(emitter, overlay.base_color.value);
  }
  emitOptionalFloat(emitter, "metallic", overlay.metallic);
  emitOptionalFloat(emitter, "roughness", overlay.roughness);
  if (overlay.ambient.present) {
    emitter << YAML::Key << "ambient" << YAML::Value;
    emitVec3(emitter, overlay.ambient.value);
  }
  if (overlay.diffuse.present) {
    emitter << YAML::Key << "diffuse" << YAML::Value;
    emitVec3(emitter, overlay.diffuse.value);
  }
  if (overlay.specular.present) {
    emitter << YAML::Key << "specular" << YAML::Value;
    emitVec3(emitter, overlay.specular.value);
  }
  emitOptionalFloat(emitter, "shininess", overlay.shininess);
  if (overlay.base_color_texture.present ||
      overlay.metallic_roughness_texture.present ||
      overlay.normal_texture.present || overlay.occlusion_texture.present) {
    emitter << YAML::Key << "textures" << YAML::Value << YAML::BeginMap;
    emitOptionalSlot(emitter, "base_color", overlay.base_color_texture);
    emitOptionalSlot(emitter, "metallic_roughness",
                     overlay.metallic_roughness_texture);
    emitOptionalSlot(emitter, "normal", overlay.normal_texture);
    emitOptionalSlot(emitter, "occlusion", overlay.occlusion_texture);
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndMap;
}

}  // namespace

bool AssetYaml::parseMeshDescriptor(const eastl::string& yaml_text,
                                    MeshAssetDescriptor& out_descriptor) {
  try {
    const YAML::Node root = loadRoot(yaml_text);
    if (!root || !root.IsMap()) {
      return false;
    }

    out_descriptor = {};

    const YAML::Node type_node = root["type"];
    if (!type_node || type_node.as<std::string>() != "Mesh") {
      return false;
    }

    if (!readStringField(root, "guid", out_descriptor.guid)) {
      return false;
    }
    if (!readStringField(root, "source", out_descriptor.source)) {
      return false;
    }
    readOptionalStringField(root, "archived_source",
                            out_descriptor.archived_source);

    readGuidSequence(root["texture_guids"], out_descriptor.texture_guids);
    readGuidSequence(root["import_texture_guids"],
                     out_descriptor.import_texture_guids);
    parseMaterialOverride(root["material_override"],
                          out_descriptor.material_override);
    if (out_descriptor.import_texture_guids.empty()) {
      out_descriptor.import_texture_guids = out_descriptor.texture_guids;
    }

    out_descriptor.unknown_root_fields.clear();
    for (const auto& kv : root) {
      if (!kv.first || !kv.first.IsScalar()) {
        continue;
      }
      const std::string key = kv.first.as<std::string>();
      if (isKnownMeshRootKey(key)) {
        continue;
      }
      out_descriptor.unknown_root_fields.push_back(
          {eastl::string(key.c_str()),
           eastl::string(YAML::Dump(kv.second).c_str())});
    }

    out_descriptor.companion_animation_sources.clear();
    const YAML::Node companion_sources =
        root["companion_animation_sources"];
    if (companion_sources && companion_sources.IsSequence()) {
      for (const auto& item : companion_sources) {
        if (!item || !item.IsScalar()) {
          continue;
        }
        const eastl::string source = item.as<std::string>().c_str();
        if (!source.empty()) {
          out_descriptor.companion_animation_sources.push_back(source);
        }
      }
    }

    const YAML::Node import = root["import"];
    if (import && import.IsMap()) {
      readBoolField(import, "materials", true,
                    out_descriptor.import.materials);
      readBoolField(import, "animations", true,
                    out_descriptor.import.animations);
      readFloatField(import, "scale", 1.0f, out_descriptor.import.scale);
    }
    return true;
  } catch (const YAML::Exception& exception) {
    LOG_WARN("[AssetYaml] parseMeshDescriptor failed: {}", exception.what());
    return false;
  }
}

bool AssetYaml::parseTextureDescriptor(const eastl::string& yaml_text,
                                       TextureAssetDescriptor& out_descriptor) {
  try {
    const YAML::Node root = loadRoot(yaml_text);
    if (!root || !root.IsMap()) {
      return false;
    }

    const YAML::Node type_node = root["type"];
    if (!type_node || type_node.as<std::string>() != "Texture2D") {
      return false;
    }

    if (!readStringField(root, "guid", out_descriptor.guid)) {
      return false;
    }
    if (!readStringField(root, "source", out_descriptor.source)) {
      return false;
    }
    readOptionalStringField(root, "archived_source",
                            out_descriptor.archived_source);

    const YAML::Node import = root["import"];
    if (import && import.IsMap()) {
      readBoolField(import, "srgb", true, out_descriptor.import.srgb);
      readBoolField(import, "generate_mips", false,
                    out_descriptor.import.generate_mips);
    }
    return true;
  } catch (const YAML::Exception& exception) {
    LOG_WARN("[AssetYaml] parseTextureDescriptor failed: {}", exception.what());
    return false;
  }
}

eastl::string AssetYaml::serializeMeshDescriptor(
    const MeshAssetDescriptor& descriptor) {
  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "type" << YAML::Value << "Mesh";
  emitter << YAML::Key << "guid" << YAML::Value << descriptor.guid.c_str();
  emitter << YAML::Key << "source" << YAML::Value << descriptor.source.c_str();
  if (!descriptor.archived_source.empty()) {
    emitter << YAML::Key << "archived_source" << YAML::Value
            << descriptor.archived_source.c_str();
  }
  writeGuidSequence(emitter, "texture_guids", descriptor.texture_guids);
  if (!descriptor.import_texture_guids.empty() &&
      (!descriptor.material_override.empty() ||
       descriptor.import_texture_guids != descriptor.texture_guids)) {
    writeGuidSequence(emitter, "import_texture_guids",
                      descriptor.import_texture_guids);
  }
  if (!descriptor.companion_animation_sources.empty()) {
    emitter << YAML::Key << "companion_animation_sources" << YAML::Value
            << YAML::BeginSeq;
    for (const eastl::string& source :
         descriptor.companion_animation_sources) {
      emitter << source.c_str();
    }
    emitter << YAML::EndSeq;
  }
  emitter << YAML::Key << "import" << YAML::Value << YAML::BeginMap;
  emitter << YAML::Key << "materials" << YAML::Value
          << descriptor.import.materials;
  emitter << YAML::Key << "animations" << YAML::Value
          << descriptor.import.animations;
  emitter << YAML::Key << "scale" << YAML::Value << descriptor.import.scale;
  emitter << YAML::EndMap;
  serializeMaterialOverride(emitter, descriptor.material_override);
  for (const auto& field : descriptor.unknown_root_fields) {
    emitter << YAML::Key << field.first.c_str();
    try {
      emitter << YAML::Load(field.second.c_str());
    } catch (const YAML::Exception&) {
      emitter << field.second.c_str();
    }
  }
  emitter << YAML::EndMap;
  return eastl::string(emitter.c_str());
}

eastl::string AssetYaml::serializeTextureDescriptor(
    const TextureAssetDescriptor& descriptor) {
  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "type" << YAML::Value << "Texture2D";
  emitter << YAML::Key << "guid" << YAML::Value << descriptor.guid.c_str();
  emitter << YAML::Key << "source" << YAML::Value << descriptor.source.c_str();
  if (!descriptor.archived_source.empty()) {
    emitter << YAML::Key << "archived_source" << YAML::Value
            << descriptor.archived_source.c_str();
  }
  emitter << YAML::Key << "import" << YAML::Value << YAML::BeginMap;
  emitter << YAML::Key << "srgb" << YAML::Value << descriptor.import.srgb;
  emitter << YAML::Key << "generate_mips" << YAML::Value
          << descriptor.import.generate_mips;
  emitter << YAML::EndMap;
  emitter << YAML::EndMap;
  return eastl::string(emitter.c_str());
}

bool AssetYaml::parseSourceField(const eastl::string& yaml_text,
                                 eastl::string& out_source) {
  try {
    const YAML::Node root = loadRoot(yaml_text);
    return readStringField(root, "source", out_source);
  } catch (const YAML::Exception& exception) {
    LOG_WARN("[AssetYaml] parseSourceField failed: {}", exception.what());
    return false;
  }
}

namespace {

bool parseAnimationChannel(const std::string& text, AnimationChannel& out_channel) {
  if (text == "translation") {
    out_channel = AnimationChannel::Translation;
    return true;
  }
  if (text == "rotation") {
    out_channel = AnimationChannel::Rotation;
    return true;
  }
  if (text == "scale") {
    out_channel = AnimationChannel::Scale;
    return true;
  }
  return false;
}

const char* animationChannelToString(AnimationChannel channel) {
  switch (channel) {
    case AnimationChannel::Translation:
      return "translation";
    case AnimationChannel::Rotation:
      return "rotation";
    case AnimationChannel::Scale:
      return "scale";
  }
  return "translation";
}

bool parseAnimationInterpolation(const std::string& text,
                                 AnimationInterpolation& out_interpolation) {
  if (text == "Constant") {
    out_interpolation = AnimationInterpolation::Constant;
    return true;
  }
  if (text == "Linear") {
    out_interpolation = AnimationInterpolation::Linear;
    return true;
  }
  return false;
}

const char* animationInterpolationToString(AnimationInterpolation interpolation) {
  switch (interpolation) {
    case AnimationInterpolation::Constant:
      return "Constant";
    case AnimationInterpolation::Linear:
      return "Linear";
  }
  return "Constant";
}

bool readKeyframeValue(const YAML::Node& value_node, AnimationChannel channel,
                       eastl::vector<float>& out_value) {
  if (!value_node || !value_node.IsSequence()) {
    return false;
  }
  const size_t expected =
      channel == AnimationChannel::Rotation ? 4u : 3u;
  if (value_node.size() != expected) {
    return false;
  }
  out_value.clear();
  out_value.reserve(expected);
  for (const auto& item : value_node) {
    if (!item || !item.IsScalar()) {
      return false;
    }
    out_value.push_back(item.as<float>());
  }
  return true;
}

bool validateTrackKeyTimes(const AnimationTrack& track) {
  float previous_time = -1.0f;
  float last_key_time = 0.0f;
  for (const AnimationKeyframe& key : track.keys) {
    if (key.time < previous_time) {
      return false;
    }
    previous_time = key.time;
    last_key_time = key.time;
  }
  (void)last_key_time;
  return true;
}

bool validateClipDuration(const AnimationClipData& data) {
  float last_key_time = 0.0f;
  for (const AnimationTrack& track : data.tracks) {
    if (!track.keys.empty()) {
      const float track_last = track.keys.back().time;
      if (track_last > last_key_time) {
        last_key_time = track_last;
      }
    }
  }
  for (const AnimationMethodKey& key : data.method_keys) {
    if (key.time > last_key_time) {
      last_key_time = key.time;
    }
  }
  if (data.tracks.empty() && data.method_keys.empty()) {
    return data.duration >= 0.0f;
  }
  return data.duration >= last_key_time;
}

bool parseMethodKeys(const YAML::Node& keys_node,
                     eastl::vector<AnimationMethodKey>& out_keys) {
  if (!keys_node || !keys_node.IsSequence()) {
    return false;
  }

  out_keys.clear();
  float previous_time = -1.0f;
  for (const auto& key_node : keys_node) {
    if (!key_node || !key_node.IsMap()) {
      return false;
    }

    AnimationMethodKey key;
    if (!readStringField(key_node, "name", key.name)) {
      return false;
    }

    const YAML::Node time_node = key_node["time"];
    if (!time_node || !time_node.IsScalar()) {
      return false;
    }
    key.time = time_node.as<float>();
    if (key.time < previous_time) {
      return false;
    }
    previous_time = key.time;

    const YAML::Node args_node = key_node["args"];
    if (args_node && args_node.IsSequence()) {
      for (const auto& arg_node : args_node) {
        if (!arg_node || !arg_node.IsScalar()) {
          return false;
        }
        key.args.push_back(arg_node.as<float>());
      }
    }

    out_keys.push_back(key);
  }
  return true;
}

}  // namespace

bool AssetYaml::parseAnimationClipDescriptor(
    const eastl::string& yaml_text,
    AnimationClipAssetDescriptor& out_descriptor) {
  try {
    const YAML::Node root = loadRoot(yaml_text);
    if (!root || !root.IsMap()) {
      return false;
    }

    const YAML::Node type_node = root["type"];
    if (!type_node || type_node.as<std::string>() != "AnimationClip") {
      return false;
    }

    if (!readStringField(root, "guid", out_descriptor.guid)) {
      return false;
    }
    if (!readStringField(root, "source", out_descriptor.source)) {
      return false;
    }
    readOptionalStringField(root, "archived_source",
                            out_descriptor.archived_source);
    return true;
  } catch (const YAML::Exception& exception) {
    LOG_WARN("[AssetYaml] parseAnimationClipDescriptor failed: {}",
             exception.what());
    return false;
  }
}

eastl::string AssetYaml::serializeAnimationClipDescriptor(
    const AnimationClipAssetDescriptor& descriptor) {
  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "type" << YAML::Value << "AnimationClip";
  emitter << YAML::Key << "guid" << YAML::Value << descriptor.guid.c_str();
  emitter << YAML::Key << "source" << YAML::Value << descriptor.source.c_str();
  if (!descriptor.archived_source.empty()) {
    emitter << YAML::Key << "archived_source" << YAML::Value
            << descriptor.archived_source.c_str();
  }
  emitter << YAML::EndMap;
  return eastl::string(emitter.c_str());
}

bool AssetYaml::parseAnimationClipData(const eastl::string& yaml_text,
                                       AnimationClipData& out_data) {
  try {
    const YAML::Node root = loadRoot(yaml_text);
    if (!root || !root.IsMap()) {
      return false;
    }

    const YAML::Node version_node = root["version"];
    if (!version_node || !version_node.IsScalar() ||
        version_node.as<int>() != AnimationClipData::kVersion) {
      return false;
    }

    if (!readStringField(root, "name", out_data.name)) {
      return false;
    }

    const YAML::Node duration_node = root["duration"];
    if (!duration_node || !duration_node.IsScalar()) {
      return false;
    }
    out_data.duration = duration_node.as<float>();

    out_data.tracks.clear();
    const YAML::Node tracks = root["tracks"];
    if (!tracks || !tracks.IsSequence()) {
      return false;
    }

    for (const auto& track_node : tracks) {
      if (!track_node || !track_node.IsMap()) {
        return false;
      }

      AnimationTrack track;
      if (!readStringField(track_node, "bone", track.bone)) {
        return false;
      }

      const YAML::Node channel_node = track_node["channel"];
      if (!channel_node || !channel_node.IsScalar() ||
          !parseAnimationChannel(channel_node.as<std::string>(), track.channel)) {
        return false;
      }

      const YAML::Node interpolation_node = track_node["interpolation"];
      if (!interpolation_node || !interpolation_node.IsScalar() ||
          !parseAnimationInterpolation(interpolation_node.as<std::string>(),
                                       track.interpolation)) {
        return false;
      }

      const YAML::Node keys = track_node["keys"];
      if (!keys || !keys.IsSequence()) {
        return false;
      }

      for (const auto& key_node : keys) {
        if (!key_node || !key_node.IsMap()) {
          return false;
        }

        const YAML::Node time_node = key_node["time"];
        if (!time_node || !time_node.IsScalar()) {
          return false;
        }

        AnimationKeyframe key;
        key.time = time_node.as<float>();
        if (!readKeyframeValue(key_node["value"], track.channel, key.value)) {
          return false;
        }
        track.keys.push_back(key);
      }

      if (!validateTrackKeyTimes(track)) {
        return false;
      }
      out_data.tracks.push_back(track);
    }

    out_data.method_keys.clear();
    const YAML::Node method_keys = root["method_keys"];
    if (method_keys) {
      if (!parseMethodKeys(method_keys, out_data.method_keys)) {
        return false;
      }
    }

    if (!validateClipDuration(out_data)) {
      return false;
    }
    return true;
  } catch (const YAML::Exception& exception) {
    LOG_WARN("[AssetYaml] parseAnimationClipData failed: {}", exception.what());
    return false;
  }
}

eastl::string AssetYaml::serializeAnimationClipData(
    const AnimationClipData& data) {
  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "version" << YAML::Value << AnimationClipData::kVersion;
  emitter << YAML::Key << "name" << YAML::Value << data.name.c_str();
  emitter << YAML::Key << "duration" << YAML::Value << data.duration;
  emitter << YAML::Key << "tracks" << YAML::Value << YAML::BeginSeq;
  for (const AnimationTrack& track : data.tracks) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "bone" << YAML::Value << track.bone.c_str();
    emitter << YAML::Key << "channel" << YAML::Value
            << animationChannelToString(track.channel);
    emitter << YAML::Key << "interpolation" << YAML::Value
            << animationInterpolationToString(track.interpolation);
    emitter << YAML::Key << "keys" << YAML::Value << YAML::BeginSeq;
    for (const AnimationKeyframe& key : track.keys) {
      emitter << YAML::BeginMap;
      emitter << YAML::Key << "time" << YAML::Value << key.time;
      emitter << YAML::Key << "value" << YAML::Value << YAML::BeginSeq;
      for (float component : key.value) {
        emitter << component;
      }
      emitter << YAML::EndSeq;
      emitter << YAML::EndMap;
    }
    emitter << YAML::EndSeq;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;
  if (!data.method_keys.empty()) {
    emitter << YAML::Key << "method_keys" << YAML::Value << YAML::BeginSeq;
    for (const AnimationMethodKey& key : data.method_keys) {
      emitter << YAML::BeginMap;
      emitter << YAML::Key << "name" << YAML::Value << key.name.c_str();
      emitter << YAML::Key << "time" << YAML::Value << key.time;
      if (!key.args.empty()) {
        emitter << YAML::Key << "args" << YAML::Value << YAML::BeginSeq;
        for (float arg : key.args) {
          emitter << arg;
        }
        emitter << YAML::EndSeq;
      }
      emitter << YAML::EndMap;
    }
    emitter << YAML::EndSeq;
  }
  emitter << YAML::EndMap;
  return eastl::string(emitter.c_str());
}

bool AssetYaml::parseAnimationTreeAssetDescriptor(
    const eastl::string& yaml_text,
    AnimationTreeAssetDescriptor& out_descriptor) {
  try {
    const YAML::Node root = loadRoot(yaml_text);
    if (!root || !root.IsMap()) {
      return false;
    }
    const YAML::Node type_node = root["type"];
    if (!type_node || type_node.as<std::string>() != "AnimationTree") {
      return false;
    }
    if (!readStringField(root, "guid", out_descriptor.guid)) {
      return false;
    }
    if (!readStringField(root, "source", out_descriptor.source)) {
      return false;
    }
    readOptionalStringField(root, "archived_source",
                            out_descriptor.archived_source);
    return true;
  } catch (const YAML::Exception& exception) {
    LOG_WARN("[AssetYaml] parseAnimationTreeAssetDescriptor failed: {}",
             exception.what());
    return false;
  }
}

eastl::string AssetYaml::serializeAnimationTreeAssetDescriptor(
    const AnimationTreeAssetDescriptor& descriptor) {
  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "type" << YAML::Value << "AnimationTree";
  emitter << YAML::Key << "guid" << YAML::Value << descriptor.guid.c_str();
  emitter << YAML::Key << "source" << YAML::Value << descriptor.source.c_str();
  if (!descriptor.archived_source.empty()) {
    emitter << YAML::Key << "archived_source" << YAML::Value
            << descriptor.archived_source.c_str();
  }
  emitter << YAML::EndMap;
  return eastl::string(emitter.c_str());
}

bool AssetYaml::parseAnimationTreeTopologyData(
    const eastl::string& yaml_text, AnimationTreeTopologyData& out_data) {
  try {
    const YAML::Node root = loadRoot(yaml_text);
    if (!root || !root.IsMap()) {
      return false;
    }
    const YAML::Node version_node = root["version"];
    if (!version_node || !version_node.IsScalar()) {
      return false;
    }
    const int version = version_node.as<int>();
    if (version != 1 && version != AnimationTreeTopologyData::kVersion) {
      return false;
    }

    out_data = AnimationTreeTopologyData{};
    readOptionalStringField(root, "base_blend_space_node",
                            out_data.base_blend_space_node);
    readOptionalStringField(root, "base_blend_space_2d_node",
                            out_data.base_blend_space_2d_node);
    readOptionalStringField(root, "add2_clip", out_data.add2_clip);
    readOptionalStringField(root, "oneshot_clip", out_data.oneshot_clip);

    const YAML::Node spaces1d = root["blend_spaces_1d"];
    if (spaces1d && spaces1d.IsSequence()) {
      for (const auto& space_node : spaces1d) {
        if (!space_node || !space_node.IsMap()) {
          return false;
        }
        AnimationTreeTopologyData::BlendSpace1DDef space;
        if (!readStringField(space_node, "node_name", space.node_name)) {
          return false;
        }
        readFloatField(space_node, "scalar", 0.0f, space.scalar);
        const YAML::Node points = space_node["points"];
        if (points && points.IsSequence()) {
          for (const auto& point_node : points) {
            AnimationTreeTopologyData::BlendSpace1DPointDef point;
            if (!readStringField(point_node, "clip_name", point.clip_name)) {
              return false;
            }
            readFloatField(point_node, "scalar", 0.0f, point.scalar);
            space.points.push_back(eastl::move(point));
          }
        }
        out_data.blend_spaces_1d.push_back(eastl::move(space));
      }
    }

    const YAML::Node spaces2d = root["blend_spaces_2d"];
    if (spaces2d && spaces2d.IsSequence()) {
      for (const auto& space_node : spaces2d) {
        if (!space_node || !space_node.IsMap()) {
          return false;
        }
        AnimationTreeTopologyData::BlendSpace2DDef space;
        if (!readStringField(space_node, "node_name", space.node_name)) {
          return false;
        }
        readFloatField(space_node, "x", 0.0f, space.x);
        readFloatField(space_node, "y", 0.0f, space.y);
        const YAML::Node points = space_node["points"];
        if (points && points.IsSequence()) {
          for (const auto& point_node : points) {
            AnimationTreeTopologyData::BlendSpace2DPointDef point;
            if (!readStringField(point_node, "clip_name", point.clip_name)) {
              return false;
            }
            readFloatField(point_node, "x", 0.0f, point.x);
            readFloatField(point_node, "y", 0.0f, point.y);
            space.points.push_back(eastl::move(point));
          }
        }
        out_data.blend_spaces_2d.push_back(eastl::move(space));
      }
    }

    const YAML::Node states = root["states"];
    if (states && states.IsSequence()) {
      for (const auto& state_node : states) {
        if (!state_node || !state_node.IsMap()) {
          return false;
        }
        AnimationTreeTopologyData::StateDef state;
        if (!readStringField(state_node, "name", state.name)) {
          return false;
        }
        readOptionalStringField(state_node, "kind", state.kind);
        if (state.kind.empty()) {
          state.kind = "clip";
        }
        readOptionalStringField(state_node, "clip_name", state.clip_name);
        readOptionalStringField(state_node, "blend_space_node",
                                state.blend_space_node);
        out_data.states.push_back(eastl::move(state));
      }
    }

    const YAML::Node tree_params = root["tree_params"];
    if (tree_params && tree_params.IsSequence()) {
      for (const auto& param_node : tree_params) {
        if (!param_node || !param_node.IsMap()) {
          return false;
        }
        AnimationTreeTopologyData::TreeParamDef param;
        if (!readStringField(param_node, "name", param.name)) {
          return false;
        }
        readOptionalStringField(param_node, "kind", param.kind);
        if (param.kind.empty()) {
          param.kind = "float";
        }
        bool bool_default = false;
        if (param_node["bool_default"] && param_node["bool_default"].IsScalar()) {
          bool_default = param_node["bool_default"].as<bool>();
        }
        param.bool_default = bool_default;
        readFloatField(param_node, "float_default", 0.0f, param.float_default);
        out_data.tree_params.push_back(eastl::move(param));
      }
    }

    const YAML::Node transitions = root["transitions"];
    if (transitions && transitions.IsSequence()) {
      for (const auto& edge_node : transitions) {
        if (!edge_node || !edge_node.IsMap()) {
          return false;
        }
        AnimationTreeTopologyData::TransitionDef edge;
        if (!readStringField(edge_node, "from_state", edge.from_state) ||
            !readStringField(edge_node, "to_state", edge.to_state) ||
            !readStringField(edge_node, "param_name", edge.param_name)) {
          return false;
        }
        readOptionalStringField(edge_node, "source", edge.source);
        if (edge.source.empty()) {
          edge.source = "treeParam";
        }
        readOptionalStringField(edge_node, "op", edge.op);
        if (edge.op.empty()) {
          edge.op = "eq";
        }
        if (edge_node["is_bool_predicate"] &&
            edge_node["is_bool_predicate"].IsScalar()) {
          edge.is_bool_predicate = edge_node["is_bool_predicate"].as<bool>();
        }
        if (edge_node["bool_operand"] && edge_node["bool_operand"].IsScalar()) {
          edge.bool_operand = edge_node["bool_operand"].as<bool>();
        }
        readFloatField(edge_node, "float_operand", 0.0f, edge.float_operand);
        if (edge_node["priority"] && edge_node["priority"].IsScalar()) {
          edge.priority = edge_node["priority"].as<int>();
        }
        out_data.transitions.push_back(eastl::move(edge));
      }
    }

    const YAML::Node layout = root["canvas_layout"];
    if (layout && layout.IsSequence()) {
      for (const auto& node : layout) {
        if (!node || !node.IsMap()) {
          return false;
        }
        AnimationTreeTopologyData::CanvasLayoutNodeDef item;
        if (!readStringField(node, "node_id", item.node_id)) {
          return false;
        }
        readFloatField(node, "x", 0.0f, item.x);
        readFloatField(node, "y", 0.0f, item.y);
        out_data.canvas_layout.push_back(eastl::move(item));
      }
    }

    return true;
  } catch (const YAML::Exception& exception) {
    LOG_WARN("[AssetYaml] parseAnimationTreeTopologyData failed: {}",
             exception.what());
    return false;
  }
}

eastl::string AssetYaml::serializeAnimationTreeTopologyData(
    const AnimationTreeTopologyData& data) {
  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "version" << YAML::Value
          << AnimationTreeTopologyData::kVersion;
  if (!data.base_blend_space_node.empty()) {
    emitter << YAML::Key << "base_blend_space_node" << YAML::Value
            << data.base_blend_space_node.c_str();
  }
  if (!data.base_blend_space_2d_node.empty()) {
    emitter << YAML::Key << "base_blend_space_2d_node" << YAML::Value
            << data.base_blend_space_2d_node.c_str();
  }
  if (!data.add2_clip.empty()) {
    emitter << YAML::Key << "add2_clip" << YAML::Value << data.add2_clip.c_str();
  }
  if (!data.oneshot_clip.empty()) {
    emitter << YAML::Key << "oneshot_clip" << YAML::Value
            << data.oneshot_clip.c_str();
  }

  emitter << YAML::Key << "blend_spaces_1d" << YAML::Value << YAML::BeginSeq;
  for (const AnimationTreeTopologyData::BlendSpace1DDef& space :
       data.blend_spaces_1d) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "node_name" << YAML::Value
            << space.node_name.c_str();
    emitter << YAML::Key << "scalar" << YAML::Value << space.scalar;
    emitter << YAML::Key << "points" << YAML::Value << YAML::BeginSeq;
    for (const AnimationTreeTopologyData::BlendSpace1DPointDef& point :
         space.points) {
      emitter << YAML::BeginMap;
      emitter << YAML::Key << "clip_name" << YAML::Value
              << point.clip_name.c_str();
      emitter << YAML::Key << "scalar" << YAML::Value << point.scalar;
      emitter << YAML::EndMap;
    }
    emitter << YAML::EndSeq;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::Key << "blend_spaces_2d" << YAML::Value << YAML::BeginSeq;
  for (const AnimationTreeTopologyData::BlendSpace2DDef& space :
       data.blend_spaces_2d) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "node_name" << YAML::Value
            << space.node_name.c_str();
    emitter << YAML::Key << "x" << YAML::Value << space.x;
    emitter << YAML::Key << "y" << YAML::Value << space.y;
    emitter << YAML::Key << "points" << YAML::Value << YAML::BeginSeq;
    for (const AnimationTreeTopologyData::BlendSpace2DPointDef& point :
         space.points) {
      emitter << YAML::BeginMap;
      emitter << YAML::Key << "clip_name" << YAML::Value
              << point.clip_name.c_str();
      emitter << YAML::Key << "x" << YAML::Value << point.x;
      emitter << YAML::Key << "y" << YAML::Value << point.y;
      emitter << YAML::EndMap;
    }
    emitter << YAML::EndSeq;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::Key << "states" << YAML::Value << YAML::BeginSeq;
  for (const AnimationTreeTopologyData::StateDef& state : data.states) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "name" << YAML::Value << state.name.c_str();
    emitter << YAML::Key << "kind" << YAML::Value << state.kind.c_str();
    if (!state.clip_name.empty()) {
      emitter << YAML::Key << "clip_name" << YAML::Value
              << state.clip_name.c_str();
    }
    if (!state.blend_space_node.empty()) {
      emitter << YAML::Key << "blend_space_node" << YAML::Value
              << state.blend_space_node.c_str();
    }
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::Key << "tree_params" << YAML::Value << YAML::BeginSeq;
  for (const AnimationTreeTopologyData::TreeParamDef& param : data.tree_params) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "name" << YAML::Value << param.name.c_str();
    emitter << YAML::Key << "kind" << YAML::Value << param.kind.c_str();
    if (param.kind == "bool") {
      emitter << YAML::Key << "bool_default" << YAML::Value
              << param.bool_default;
    } else {
      emitter << YAML::Key << "float_default" << YAML::Value
              << param.float_default;
    }
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::Key << "transitions" << YAML::Value << YAML::BeginSeq;
  for (const AnimationTreeTopologyData::TransitionDef& edge :
       data.transitions) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "from_state" << YAML::Value
            << edge.from_state.c_str();
    emitter << YAML::Key << "to_state" << YAML::Value << edge.to_state.c_str();
    emitter << YAML::Key << "source" << YAML::Value << edge.source.c_str();
    emitter << YAML::Key << "param_name" << YAML::Value
            << edge.param_name.c_str();
    emitter << YAML::Key << "is_bool_predicate" << YAML::Value
            << edge.is_bool_predicate;
    emitter << YAML::Key << "op" << YAML::Value << edge.op.c_str();
    emitter << YAML::Key << "float_operand" << YAML::Value
            << edge.float_operand;
    emitter << YAML::Key << "bool_operand" << YAML::Value << edge.bool_operand;
    emitter << YAML::Key << "priority" << YAML::Value << edge.priority;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::Key << "canvas_layout" << YAML::Value << YAML::BeginSeq;
  for (const AnimationTreeTopologyData::CanvasLayoutNodeDef& node :
       data.canvas_layout) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "node_id" << YAML::Value << node.node_id.c_str();
    emitter << YAML::Key << "x" << YAML::Value << node.x;
    emitter << YAML::Key << "y" << YAML::Value << node.y;
    emitter << YAML::EndMap;
  }
  emitter << YAML::EndSeq;

  emitter << YAML::EndMap;
  return eastl::string(emitter.c_str());
}

}  // namespace Blunder
