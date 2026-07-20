#include "runtime/resource/asset_registry/asset_registry.h"

#include <cstring>
#include <filesystem>

#include <yaml-cpp/yaml.h>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/guid.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {

bool endsWith(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
         0;
}

bool startsWithPrefix(const eastl::string& value, const char* prefix) {
  const size_t prefix_length = std::strlen(prefix);
  if (value.size() < prefix_length) {
    return false;
  }
  return value.compare(0, prefix_length, prefix) == 0;
}

bool isScannableDescriptor(const eastl::string& virtual_path) {
  return endsWith(virtual_path, ".mesh.yaml") ||
         endsWith(virtual_path, ".texture.yaml") ||
         endsWith(virtual_path, ".scene.asset");
}

bool parseGuidFromDocument(const eastl::string& text, eastl::string& out_guid) {
  out_guid.clear();
  try {
    const YAML::Node root = YAML::Load(text.c_str());
    const YAML::Node guid_node = root["guid"];
    if (guid_node && guid_node.IsScalar()) {
      out_guid = guid_node.as<std::string>().c_str();
    }
  } catch (const YAML::Exception&) {
    return false;
  }
  return !out_guid.empty() && isValidGuidFormat(out_guid);
}

/// Inserts or replaces the Scene JSON `"guid"` field with `guid`.
bool upsertGuidIntoSceneJson(eastl::string& json_text,
                             const eastl::string& guid) {
  const char* guid_key = "\"guid\"";
  const size_t guid_key_pos = json_text.find(guid_key);
  if (guid_key_pos != eastl::string::npos) {
    const size_t colon = json_text.find(':', guid_key_pos + std::strlen(guid_key));
    if (colon == eastl::string::npos) {
      return false;
    }
    const size_t quote_start = json_text.find('"', colon + 1);
    if (quote_start == eastl::string::npos) {
      return false;
    }
    const size_t quote_end = json_text.find('"', quote_start + 1);
    if (quote_end == eastl::string::npos) {
      return false;
    }
    json_text.replace(quote_start + 1, quote_end - quote_start - 1, guid);
    return true;
  }

  const char* type_key = "\"type\"";
  const size_t type_pos = json_text.find(type_key);
  if (type_pos == eastl::string::npos) {
    return false;
  }

  const size_t comma_after_type = json_text.find(',', type_pos);
  if (comma_after_type == eastl::string::npos) {
    // `{ "type": "Scene" }` with no trailing fields — insert before closing `}`.
    const size_t close_brace = json_text.rfind('}');
    if (close_brace == eastl::string::npos) {
      return false;
    }
    eastl::string insertion = ",\n  \"guid\": \"";
    insertion.append(guid);
    insertion.append("\"");
    json_text.insert(close_brace, insertion);
    return true;
  }

  eastl::string insertion = "\n  \"guid\": \"";
  insertion.append(guid);
  insertion.append("\",");
  json_text.insert(comma_after_type + 1, insertion);
  return true;
}

fs::path resolveSceneAbsolutePath(FileSystem* file_system,
                                  const eastl::string& scene_virtual_path) {
  eastl::string normalized(fs::path(scene_virtual_path.c_str())
                               .lexically_normal()
                               .generic_string()
                               .c_str());
  if (startsWithPrefix(normalized, "assets/")) {
    const eastl::string relative =
        eastl::string(normalized.c_str() + std::strlen("assets/"));
    return file_system->resolveAsset(fs::path(relative.c_str()));
  }
  return file_system->resolveAsset(fs::path(normalized.c_str()));
}

}  // namespace

void AssetRegistry::initialize(FileSystem* file_system) {
  m_file_system = file_system;
  m_is_initialized = file_system != nullptr;
  if (m_is_initialized) {
    load();
  }
}

void AssetRegistry::shutdown() {
  if (m_is_initialized) {
    save();
  }
  m_guid_to_path.clear();
  m_path_to_guid.clear();
  m_file_system = nullptr;
  m_is_initialized = false;
}

eastl::string AssetRegistry::registryPath() const {
  ASSERT(m_file_system);
  return eastl::string(
      m_file_system->resolve(".blunder/asset_registry.yaml").generic_string()
          .c_str());
}

eastl::string AssetRegistry::allocateGuid() {
  for (uint32_t attempt = 0; attempt < 32; ++attempt) {
    const eastl::string guid = generateGuidV4();
    if (m_guid_to_path.find(guid) == m_guid_to_path.end()) {
      return guid;
    }
  }
  return generateGuidV4();
}

bool AssetRegistry::registerAsset(const eastl::string& guid,
                                  const eastl::string& descriptor_virtual_path) {
  if (!m_is_initialized || guid.empty() || descriptor_virtual_path.empty()) {
    return false;
  }

  const auto existing_path = m_guid_to_path.find(guid);
  if (existing_path != m_guid_to_path.end() &&
      existing_path->second != descriptor_virtual_path) {
    m_path_to_guid.erase(existing_path->second);
  }

  const auto existing_guid = m_path_to_guid.find(descriptor_virtual_path);
  if (existing_guid != m_path_to_guid.end() &&
      existing_guid->second != guid) {
    m_guid_to_path.erase(existing_guid->second);
  }

  m_guid_to_path[guid] = descriptor_virtual_path;
  m_path_to_guid[descriptor_virtual_path] = guid;
  return save();
}

bool AssetRegistry::unregisterGuid(const eastl::string& guid) {
  const auto it = m_guid_to_path.find(guid);
  if (it == m_guid_to_path.end()) {
    return false;
  }
  m_path_to_guid.erase(it->second);
  m_guid_to_path.erase(it);
  return save();
}

eastl::string AssetRegistry::resolveGuid(const eastl::string& guid) const {
  const auto it = m_guid_to_path.find(guid);
  if (it == m_guid_to_path.end()) {
    return eastl::string();
  }
  return it->second;
}

eastl::string AssetRegistry::findGuidForPath(
    const eastl::string& descriptor_virtual_path) const {
  const auto it = m_path_to_guid.find(descriptor_virtual_path);
  if (it == m_path_to_guid.end()) {
    return eastl::string();
  }
  return it->second;
}

void AssetRegistry::rebuildFromScan() {
  if (!m_is_initialized) {
    return;
  }

  m_guid_to_path.clear();
  m_path_to_guid.clear();

  const fs::path asset_root = m_file_system->getAssetRoot();
  if (!m_file_system->isDirectory(asset_root)) {
    return;
  }

  const eastl::vector<DirectoryEntry> entries =
      m_file_system->listDirectoryRecursive(asset_root, asset_root, -1);

  for (const DirectoryEntry& entry : entries) {
    if (entry.is_directory) {
      continue;
    }

    const eastl::string virtual_path(
        (eastl::string("assets/") + entry.relative_path.c_str()).c_str());
    if (!isScannableDescriptor(virtual_path)) {
      continue;
    }

    eastl::string text;
    if (!m_file_system->readText(entry.absolute_path, text)) {
      continue;
    }

    eastl::string guid;
    if (!parseGuidFromDocument(text, guid)) {
      continue;
    }

    m_guid_to_path[guid] = virtual_path;
    m_path_to_guid[virtual_path] = guid;
  }

  save();
}

bool AssetRegistry::ensureSceneAssetRegistered(
    const eastl::string& scene_virtual_path) {
  if (!m_is_initialized || scene_virtual_path.empty() ||
      !endsWith(scene_virtual_path, ".scene.asset")) {
    return false;
  }

  const eastl::string normalized(
      fs::path(scene_virtual_path.c_str())
          .lexically_normal()
          .generic_string()
          .c_str());
  const eastl::string virtual_path =
      startsWithPrefix(normalized, "assets/")
          ? normalized
          : (eastl::string("assets/") + normalized.c_str());

  const fs::path absolute =
      resolveSceneAbsolutePath(m_file_system, virtual_path);
  eastl::string text;
  if (!m_file_system->readText(absolute, text)) {
    return false;
  }

  eastl::string guid;
  if (!parseGuidFromDocument(text, guid)) {
    guid = allocateGuid();
    if (!upsertGuidIntoSceneJson(text, guid)) {
      return false;
    }
    if (!m_file_system->writeText(absolute, text)) {
      return false;
    }
  }

  return registerAsset(guid, virtual_path);
}

eastl::vector<eastl::pair<eastl::string, eastl::string>>
AssetRegistry::registeredEntries() const {
  eastl::vector<eastl::pair<eastl::string, eastl::string>> entries;
  entries.reserve(m_guid_to_path.size());
  for (const auto& pair : m_guid_to_path) {
    entries.push_back(pair);
  }
  return entries;
}

bool AssetRegistry::save() const {
  if (!m_is_initialized) {
    return false;
  }

  YAML::Emitter emitter;
  emitter << YAML::BeginMap;
  emitter << YAML::Key << "assets" << YAML::Value << YAML::BeginMap;
  for (const auto& pair : m_guid_to_path) {
    emitter << YAML::Key << pair.first.c_str() << YAML::Value
            << pair.second.c_str();
  }
  emitter << YAML::EndMap;
  emitter << YAML::EndMap;

  const fs::path path(registryPath().c_str());
  m_file_system->ensureParentDirectory(path);
  return m_file_system->writeText(path, eastl::string(emitter.c_str()));
}

bool AssetRegistry::load() {
  if (!m_is_initialized) {
    return false;
  }

  m_guid_to_path.clear();
  m_path_to_guid.clear();

  const fs::path path(registryPath().c_str());
  if (!m_file_system->exists(path)) {
    rebuildFromScan();
    return true;
  }

  eastl::string yaml_text;
  if (!m_file_system->readText(path, yaml_text)) {
    rebuildFromScan();
    return false;
  }

  try {
    const YAML::Node root = YAML::Load(yaml_text.c_str());
    const YAML::Node assets = root["assets"];
    if (!assets || !assets.IsMap()) {
      rebuildFromScan();
      return true;
    }

    for (const auto& item : assets) {
      const eastl::string guid = item.first.as<std::string>().c_str();
      const eastl::string descriptor_path =
          item.second.as<std::string>().c_str();
      if (guid.empty() || descriptor_path.empty()) {
        continue;
      }
      m_guid_to_path[guid] = descriptor_path;
      m_path_to_guid[descriptor_path] = guid;
    }
    return true;
  } catch (const YAML::Exception& exception) {
    LOG_WARN("[AssetRegistry] load failed: {}", exception.what());
    rebuildFromScan();
    return false;
  }
}

}  // namespace Blunder
