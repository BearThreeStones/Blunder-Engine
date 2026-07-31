#include "runtime/function/editor/inspector_asset_ops.h"

#include <cstring>

#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <filesystem>

namespace Blunder {
namespace {

bool endsWithSuffix(const eastl::string& value, const char* suffix) {
  const size_t suffix_len = std::strlen(suffix);
  return value.size() >= suffix_len &&
         value.compare(value.size() - suffix_len, suffix_len, suffix) == 0;
}

eastl::string basenameFromVirtualPath(const eastl::string& virtual_path) {
  if (virtual_path.empty()) {
    return eastl::string();
  }
  size_t end = virtual_path.size();
  if (virtual_path.back() == '/') {
    end -= 1;
  }
  const size_t slash = virtual_path.find_last_of('/', end - 1);
  if (slash == eastl::string::npos) {
    return virtual_path.substr(0, end);
  }
  return virtual_path.substr(slash + 1, end - slash - 1);
}

}  // namespace

bool isMeshAssetDescriptorPath(const eastl::string& virtual_path) {
  return endsWithSuffix(virtual_path, ".mesh.yaml");
}

eastl::string meshAssetDisplayNameFromPath(const eastl::string& virtual_path) {
  return basenameFromVirtualPath(virtual_path);
}

bool resolveMeshAssetInspectorIdentity(
    const eastl::string& descriptor_virtual_path, const AssetRegistry* registry,
    FileSystem* file_system, AssetInspectorIdentity& out_identity) {
  out_identity = {};
  if (!isMeshAssetDescriptorPath(descriptor_virtual_path) || file_system == nullptr) {
    return false;
  }

  out_identity.display_name = meshAssetDisplayNameFromPath(descriptor_virtual_path);
  out_identity.type_label = eastl::string("Mesh");

  if (registry != nullptr) {
    const eastl::string registry_guid =
        registry->findGuidForPath(descriptor_virtual_path);
    if (!registry_guid.empty()) {
      out_identity.guid = registry_guid;
    }
  }

  std::filesystem::path descriptor_absolute;
  if (descriptor_virtual_path.compare(0, 7, "assets/") == 0) {
    descriptor_absolute = file_system->resolveAsset(
        std::filesystem::path(descriptor_virtual_path.c_str() + 7));
  } else if (descriptor_virtual_path.compare(0, 10, "resources/") == 0) {
    descriptor_absolute = file_system->resolveResource(
        std::filesystem::path(descriptor_virtual_path.c_str() + 10));
  } else {
    descriptor_absolute =
        file_system->resolve(std::filesystem::path(descriptor_virtual_path.c_str()));
  }

  eastl::string yaml_text;
  if (!file_system->readText(descriptor_absolute, yaml_text)) {
    return !out_identity.display_name.empty();
  }

  MeshAssetDescriptor descriptor{};
  if (AssetYaml::parseMeshDescriptor(yaml_text, descriptor)) {
    if (out_identity.guid.empty() && !descriptor.guid.empty()) {
      out_identity.guid = descriptor.guid;
    }
    if (!descriptor.source.empty()) {
      out_identity.intermediate_path = descriptor.source;
    }
  } else {
    eastl::string source_only;
    if (AssetYaml::parseSourceField(yaml_text, source_only) &&
        !source_only.empty()) {
      out_identity.intermediate_path = eastl::move(source_only);
    }
  }

  return !out_identity.display_name.empty();
}

}  // namespace Blunder
