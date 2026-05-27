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

}  // namespace Blunder
