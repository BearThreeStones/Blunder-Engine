#include "runtime/resource/asset/asset_yaml.h"

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

}  // namespace

bool AssetYaml::parseMeshDescriptor(const eastl::string& yaml_text,
                                    MeshAssetDescriptor& out_descriptor) {
  try {
    const YAML::Node root = loadRoot(yaml_text);
    if (!root || !root.IsMap()) {
      return false;
    }

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

    out_descriptor.texture_guids.clear();
    const YAML::Node texture_guids = root["texture_guids"];
    if (texture_guids && texture_guids.IsSequence()) {
      for (const auto& item : texture_guids) {
        if (!item || !item.IsScalar()) {
          continue;
        }
        const eastl::string guid = item.as<std::string>().c_str();
        if (!guid.empty()) {
          out_descriptor.texture_guids.push_back(guid);
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
  if (!descriptor.texture_guids.empty()) {
    emitter << YAML::Key << "texture_guids" << YAML::Value << YAML::BeginSeq;
    for (const eastl::string& guid : descriptor.texture_guids) {
      emitter << guid.c_str();
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
  if (data.tracks.empty()) {
    return data.duration >= 0.0f;
  }
  float last_key_time = 0.0f;
  for (const AnimationTrack& track : data.tracks) {
    if (!track.keys.empty()) {
      const float track_last = track.keys.back().time;
      if (track_last > last_key_time) {
        last_key_time = track_last;
      }
    }
  }
  return data.duration >= last_key_time;
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
  emitter << YAML::EndMap;
  return eastl::string(emitter.c_str());
}

}  // namespace Blunder
