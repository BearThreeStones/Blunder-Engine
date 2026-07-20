#include "runtime/resource/asset_dependency/asset_dependency_graph.h"

#include <cstring>
#include <filesystem>

#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset_registry/asset_registry.h"

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

fs::path resolveDescriptorAbsolute(FileSystem& file_system,
                                   const eastl::string& virtual_path) {
  eastl::string normalized(fs::path(virtual_path.c_str())
                               .lexically_normal()
                               .generic_string()
                               .c_str());
  if (startsWithPrefix(normalized, "assets/")) {
    const eastl::string relative =
        eastl::string(normalized.c_str() + std::strlen("assets/"));
    return file_system.resolveAsset(fs::path(relative.c_str()));
  }
  return file_system.resolveAsset(fs::path(normalized.c_str()));
}

}  // namespace

void AssetDependencyGraph::clear() {
  m_dependents.clear();
  m_leaves.clear();
}

void AssetDependencyGraph::rebuildFromProject(FileSystem& file_system,
                                              const AssetRegistry& registry) {
  clear();

  const eastl::vector<eastl::pair<eastl::string, eastl::string>> entries =
      registry.registeredEntries();
  for (const auto& entry : entries) {
    const eastl::string& guid = entry.first;
    const eastl::string& virtual_path = entry.second;
    if (guid.empty() || virtual_path.empty()) {
      continue;
    }

    AssetDependencyLeaves leaves;
    leaves.descriptor_virtual_path = virtual_path;

    const fs::path absolute =
        resolveDescriptorAbsolute(file_system, virtual_path);
    eastl::string text;
    if (!file_system.readText(absolute, text)) {
      m_leaves[guid] = leaves;
      continue;
    }

    if (endsWith(virtual_path, ".scene.asset")) {
      Scene scene;
      if (SceneSerializer::deserialize(text, scene)) {
        for (const SceneEntityDefinition& entity : scene.getEntities()) {
          if (isValidGuidFormat(entity.mesh_virtual_path)) {
            addDependent(entity.mesh_virtual_path, guid);
          }
        }
      }
      m_leaves[guid] = leaves;
      continue;
    }

    if (endsWith(virtual_path, ".mesh.yaml")) {
      MeshAssetDescriptor descriptor;
      if (AssetYaml::parseMeshDescriptor(text, descriptor)) {
        leaves.intermediate_source_path = descriptor.source;
        for (const eastl::string& texture_guid : descriptor.texture_guids) {
          if (isValidGuidFormat(texture_guid)) {
            addDependent(texture_guid, guid);
          }
        }
      }
      m_leaves[guid] = leaves;
      continue;
    }

    if (endsWith(virtual_path, ".texture.yaml")) {
      TextureAssetDescriptor descriptor;
      if (AssetYaml::parseTextureDescriptor(text, descriptor)) {
        leaves.intermediate_source_path = descriptor.source;
      }
      m_leaves[guid] = leaves;
    }
  }
}

eastl::vector<eastl::string> AssetDependencyGraph::dependentsOf(
    const eastl::string& guid) const {
  const auto it = m_dependents.find(guid);
  if (it == m_dependents.end()) {
    return {};
  }
  return it->second;
}

AssetDependencyLeaves AssetDependencyGraph::intermediateLeavesOf(
    const eastl::string& guid) const {
  const auto it = m_leaves.find(guid);
  if (it == m_leaves.end()) {
    return {};
  }
  return it->second;
}

void AssetDependencyGraph::addDependent(const eastl::string& dependency_guid,
                                        const eastl::string& dependent_guid) {
  if (dependency_guid.empty() || dependent_guid.empty()) {
    return;
  }
  eastl::vector<eastl::string>& list = m_dependents[dependency_guid];
  for (const eastl::string& existing : list) {
    if (existing == dependent_guid) {
      return;
    }
  }
  list.push_back(dependent_guid);
}

}  // namespace Blunder
