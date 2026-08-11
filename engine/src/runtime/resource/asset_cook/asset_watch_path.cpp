#include "runtime/resource/asset_cook/asset_watch_path.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_dependency/asset_dependency_graph.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {

std::string toLowerGeneric(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  std::replace(value.begin(), value.end(), '\\', '/');
  return value;
}

std::string normalizeAbsolute(const fs::path& path) {
  return toLowerGeneric(path.lexically_normal().generic_string());
}

std::string ensureTrailingSlash(std::string value) {
  if (!value.empty() && value.back() != '/') {
    value.push_back('/');
  }
  return value;
}

bool pathContainsTokenInsensitive(const std::string& path, const char* token) {
  const std::string lowered = toLowerGeneric(path);
  const std::string needle = toLowerGeneric(token);
  return lowered.find(needle) != std::string::npos;
}

bool isUnderRoot(const std::string& normalized_file,
                 const std::string& normalized_root_with_slash) {
  if (normalized_root_with_slash.empty()) {
    return false;
  }
  if (normalized_file ==
      normalized_root_with_slash.substr(0, normalized_root_with_slash.size() -
                                               1)) {
    return true;
  }
  return normalized_file.compare(0, normalized_root_with_slash.size(),
                                 normalized_root_with_slash) == 0;
}

eastl::string virtualPathUnderRoot(const fs::path& absolute_file_path,
                                   const fs::path& root,
                                   const char* virtual_prefix) {
  // Preserve on-disk casing for registry keys (findGuidForPath is case-sensitive).
  // Compare roots with a case-insensitive normalized form.
  const std::string file_cmp = normalizeAbsolute(absolute_file_path);
  const std::string root_cmp = ensureTrailingSlash(normalizeAbsolute(root));
  if (!isUnderRoot(file_cmp, root_cmp)) {
    return {};
  }

  std::error_code ec;
  const fs::path relative = fs::relative(absolute_file_path.lexically_normal(),
                                         root.lexically_normal(), ec);
  if (ec) {
    // Fallback: slice using original generic strings with equal prefix lengths.
    const std::string file_gen =
        absolute_file_path.lexically_normal().generic_string();
    const std::string root_gen =
        ensureTrailingSlash(root.lexically_normal().generic_string());
    if (file_gen.size() < root_gen.size()) {
      return {};
    }
    std::string rel = file_gen.substr(root_gen.size());
    while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) {
      rel.erase(rel.begin());
    }
    std::replace(rel.begin(), rel.end(), '\\', '/');
    eastl::string result(virtual_prefix);
    if (!rel.empty()) {
      result.push_back('/');
      result.append(rel.c_str());
    }
    return result;
  }

  std::string rel = relative.generic_string();
  std::replace(rel.begin(), rel.end(), '\\', '/');
  while (!rel.empty() && rel.front() == '/') {
    rel.erase(rel.begin());
  }
  if (rel == ".") {
    rel.clear();
  }
  eastl::string result(virtual_prefix);
  if (!rel.empty()) {
    result.push_back('/');
    result.append(rel.c_str());
  }
  return result;
}

bool pathsEqualNormalized(const eastl::string& a, const eastl::string& b) {
  return normalizeWatchVirtualPath(a) == normalizeWatchVirtualPath(b);
}

}  // namespace

eastl::string normalizeWatchVirtualPath(const eastl::string& path) {
  std::string value = toLowerGeneric(path.c_str());
  while (!value.empty() && value.front() == '/') {
    value.erase(value.begin());
  }
  // Strip a single leading "./"
  if (value.size() >= 2 && value[0] == '.' && value[1] == '/') {
    value.erase(0, 2);
  }
  return eastl::string(value.c_str());
}

AssetWatchPathClass classifyAssetWatchPath(
    const fs::path& absolute_file_path, const fs::path& assets_root,
    const fs::path& resources_root) {
  const std::string file_norm = normalizeAbsolute(absolute_file_path);
  if (file_norm.empty()) {
    return AssetWatchPathClass::Ignored;
  }
  if (pathContainsTokenInsensitive(file_norm, "/.blunder/") ||
      pathContainsTokenInsensitive(file_norm, ".blunder/") ||
      pathContainsTokenInsensitive(file_norm, "/.blunder")) {
    return AssetWatchPathClass::Ignored;
  }

  const std::string assets_norm =
      ensureTrailingSlash(normalizeAbsolute(assets_root));
  const std::string resources_norm =
      ensureTrailingSlash(normalizeAbsolute(resources_root));

  if (isUnderRoot(file_norm, assets_norm)) {
    return AssetWatchPathClass::AssetsTree;
  }

  if (isUnderRoot(file_norm, resources_norm)) {
    const std::string relative = file_norm.substr(resources_norm.size());
    if (relative == "source" || relative.rfind("source/", 0) == 0) {
      return AssetWatchPathClass::SourceArchive;
    }
    return AssetWatchPathClass::IntermediateResource;
  }

  return AssetWatchPathClass::Ignored;
}

eastl::vector<eastl::string> guidsToInvalidateForWatchedPath(
    AssetWatchPathClass path_class, const fs::path& absolute_file_path,
    const fs::path& assets_root, const fs::path& resources_root,
    const AssetRegistry& registry, const AssetDependencyGraph& graph) {
  eastl::vector<eastl::string> result;
  if (path_class == AssetWatchPathClass::Ignored ||
      path_class == AssetWatchPathClass::SourceArchive) {
    return result;
  }

  auto pushUnique = [&result](const eastl::string& guid) {
    if (guid.empty()) {
      return;
    }
    for (const eastl::string& existing : result) {
      if (existing == guid) {
        return;
      }
    }
    result.push_back(guid);
  };

  if (path_class == AssetWatchPathClass::AssetsTree) {
    const eastl::string virtual_path =
        virtualPathUnderRoot(absolute_file_path, assets_root, "assets");
    if (!virtual_path.empty()) {
      pushUnique(registry.findGuidForPath(virtual_path));
      // Also accept descriptor paths without the assets/ prefix if registered
      // that way (registry scan typically stores assets/...).
      const eastl::string without_prefix =
          virtual_path.size() > 7 &&
                  virtual_path.compare(0, 7, "assets/") == 0
              ? eastl::string(virtual_path.c_str() + 7)
              : eastl::string();
      if (!without_prefix.empty()) {
        pushUnique(registry.findGuidForPath(without_prefix));
      }
    }
    return result;
  }

  // IntermediateResource: match dependency-graph Intermediate leaves.
  const eastl::string intermediate_virtual =
      virtualPathUnderRoot(absolute_file_path, resources_root, "resources");
  if (intermediate_virtual.empty()) {
    return result;
  }

  const eastl::vector<eastl::pair<eastl::string, eastl::string>> entries =
      registry.registeredEntries();
  for (const auto& entry : entries) {
    const AssetDependencyLeaves leaves =
        graph.intermediateLeavesOf(entry.first);
    if (leaves.intermediate_source_path.empty()) {
      continue;
    }
    if (pathsEqualNormalized(leaves.intermediate_source_path,
                             intermediate_virtual)) {
      pushUnique(entry.first);
      continue;
    }
    // Descriptors often store "Resources/..." or "resources/..." interchangeably.
    // Also allow match without the resources/ prefix.
    const eastl::string without_prefix =
        intermediate_virtual.size() > 10 &&
                intermediate_virtual.compare(0, 10, "resources/") == 0
            ? eastl::string(intermediate_virtual.c_str() + 10)
            : eastl::string();
    if (!without_prefix.empty() &&
        pathsEqualNormalized(leaves.intermediate_source_path, without_prefix)) {
      pushUnique(entry.first);
    }
  }

  return result;
}

eastl::vector<eastl::string> guidsForArchivedSourcePath(
    const fs::path& absolute_source_path, const fs::path& resources_root,
    const AssetRegistry& registry, FileSystem& file_system) {
  eastl::vector<eastl::string> result;

  const eastl::string under_resources =
      virtualPathUnderRoot(absolute_source_path, resources_root, "resources");
  if (under_resources.empty()) {
    return result;
  }

  // Canonical forms that descriptors may use for archived_source.
  eastl::vector<eastl::string> candidates;
  candidates.push_back(under_resources);  // resources/Source/...
  if (under_resources.size() > 10 &&
      under_resources.compare(0, 10, "resources/") == 0) {
    candidates.push_back(eastl::string(under_resources.c_str() + 10));  // Source/...
  }

  auto pushUnique = [&result](const eastl::string& guid) {
    if (guid.empty()) {
      return;
    }
    for (const eastl::string& existing : result) {
      if (existing == guid) {
        return;
      }
    }
    result.push_back(guid);
  };

  auto matchesArchived = [&candidates](const eastl::string& archived) {
    if (archived.empty()) {
      return false;
    }
    const eastl::string normalized = normalizeWatchVirtualPath(archived);
    for (const eastl::string& candidate : candidates) {
      if (normalized == normalizeWatchVirtualPath(candidate)) {
        return true;
      }
    }
    return false;
  };

  const eastl::vector<eastl::pair<eastl::string, eastl::string>> entries =
      registry.registeredEntries();
  for (const auto& entry : entries) {
    const eastl::string& guid = entry.first;
    const eastl::string& virtual_path = entry.second;

    eastl::string relative = virtual_path;
    if (relative.compare(0, 7, "assets/") == 0) {
      relative.erase(0, 7);
    }
    const fs::path absolute =
        file_system.resolveAsset(fs::path(relative.c_str()));
    eastl::string yaml_text;
    if (!file_system.readText(absolute, yaml_text)) {
      continue;
    }

    if (virtual_path.size() >= 10 &&
        virtual_path.compare(virtual_path.size() - 10, 10, ".mesh.yaml") == 0) {
      MeshAssetDescriptor desc;
      if (AssetYaml::parseMeshDescriptor(yaml_text, desc) &&
          matchesArchived(desc.archived_source)) {
        pushUnique(guid);
      }
      continue;
    }
    if (virtual_path.size() >= 13 &&
        virtual_path.compare(virtual_path.size() - 13, 13, ".texture.yaml") ==
            0) {
      TextureAssetDescriptor desc;
      if (AssetYaml::parseTextureDescriptor(yaml_text, desc) &&
          matchesArchived(desc.archived_source)) {
        pushUnique(guid);
      }
      continue;
    }
    if (virtual_path.size() >= 15 &&
        virtual_path.compare(virtual_path.size() - 15, 15, ".animation.yaml") ==
            0) {
      AnimationClipAssetDescriptor desc;
      if (AssetYaml::parseAnimationClipDescriptor(yaml_text, desc) &&
          matchesArchived(desc.archived_source)) {
        pushUnique(guid);
      }
    }
  }

  return result;
}

eastl::vector<fs::path> resolveDetectionExchangePaths(
    const fs::path& absolute_file_path) {
  eastl::vector<fs::path> result;
  std::error_code ec;
  if (absolute_file_path.empty() || !fs::exists(absolute_file_path, ec)) {
    // Still attribute by path even if briefly missing mid-write.
  }

  const eastl::string ext = [&]() {
    eastl::string value(absolute_file_path.extension().generic_string().c_str());
    for (char& c : value) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
  }();

  auto pushUniquePath = [&result](const fs::path& path) {
    if (path.empty()) {
      return;
    }
    for (const fs::path& existing : result) {
      if (normalizeAbsolute(existing) == normalizeAbsolute(path)) {
        return;
      }
    }
    result.push_back(path);
  };

  if (ext == ".gltf" || ext == ".glb") {
    pushUniquePath(absolute_file_path);
    return result;
  }

  const fs::path parent = absolute_file_path.parent_path();
  const eastl::string stem(
      absolute_file_path.stem().generic_string().c_str());

  if (ext == ".bin") {
    pushUniquePath(parent / (stem + ".gltf").c_str());
    pushUniquePath(parent / (stem + ".glb").c_str());
    return result;
  }

  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
      ext == ".tga") {
    // Attribute all exchange glTF/GLB in the same directory (and textures/).
    fs::path search_dir = parent;
    const eastl::string parent_name(
        parent.filename().generic_string().c_str());
    if (parent_name == "textures" || parent_name == "Textures") {
      search_dir = parent.parent_path();
    }
    std::error_code dir_ec;
    if (fs::is_directory(search_dir, dir_ec)) {
      for (const fs::directory_entry& entry :
           fs::directory_iterator(search_dir, dir_ec)) {
        if (!entry.is_regular_file(dir_ec)) {
          continue;
        }
        eastl::string child_ext(
            entry.path().extension().generic_string().c_str());
        for (char& c : child_ext) {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (child_ext == ".gltf" || child_ext == ".glb") {
          pushUniquePath(entry.path());
        }
      }
    }
    return result;
  }

  // Other Intermediate bodies (e.g. .anim.yaml): treat as the exchange path.
  pushUniquePath(absolute_file_path);
  return result;
}

eastl::vector<eastl::string> guidsForIntermediateSourcePath(
    const fs::path& absolute_file_path, const fs::path& resources_root,
    const AssetRegistry& registry, FileSystem& file_system) {
  eastl::vector<eastl::string> result;

  const eastl::vector<fs::path> exchange_paths =
      resolveDetectionExchangePaths(absolute_file_path);
  if (exchange_paths.empty()) {
    return result;
  }

  eastl::vector<eastl::string> candidates;
  for (const fs::path& exchange : exchange_paths) {
    const eastl::string under_resources =
        virtualPathUnderRoot(exchange, resources_root, "resources");
    if (under_resources.empty()) {
      continue;
    }
    candidates.push_back(under_resources);
    if (under_resources.size() > 10 &&
        under_resources.compare(0, 10, "resources/") == 0) {
      candidates.push_back(eastl::string(under_resources.c_str() + 10));
    }
  }
  if (candidates.empty()) {
    return result;
  }

  auto pushUnique = [&result](const eastl::string& guid) {
    if (guid.empty()) {
      return;
    }
    for (const eastl::string& existing : result) {
      if (existing == guid) {
        return;
      }
    }
    result.push_back(guid);
  };

  auto matchesSource = [&candidates](const eastl::string& source) {
    if (source.empty()) {
      return false;
    }
    const eastl::string normalized = normalizeWatchVirtualPath(source);
    for (const eastl::string& candidate : candidates) {
      if (normalized == normalizeWatchVirtualPath(candidate)) {
        return true;
      }
    }
    return false;
  };

  const eastl::vector<eastl::pair<eastl::string, eastl::string>> entries =
      registry.registeredEntries();
  for (const auto& entry : entries) {
    const eastl::string& guid = entry.first;
    const eastl::string& virtual_path = entry.second;

    eastl::string relative = virtual_path;
    if (relative.compare(0, 7, "assets/") == 0) {
      relative.erase(0, 7);
    }
    const fs::path absolute =
        file_system.resolveAsset(fs::path(relative.c_str()));
    eastl::string yaml_text;
    if (!file_system.readText(absolute, yaml_text)) {
      continue;
    }

    if (virtual_path.size() >= 10 &&
        virtual_path.compare(virtual_path.size() - 10, 10, ".mesh.yaml") == 0) {
      MeshAssetDescriptor desc;
      if (AssetYaml::parseMeshDescriptor(yaml_text, desc) &&
          matchesSource(desc.source)) {
        pushUnique(guid);
      }
      continue;
    }
    if (virtual_path.size() >= 13 &&
        virtual_path.compare(virtual_path.size() - 13, 13, ".texture.yaml") ==
            0) {
      TextureAssetDescriptor desc;
      if (AssetYaml::parseTextureDescriptor(yaml_text, desc) &&
          matchesSource(desc.source)) {
        pushUnique(guid);
      }
      continue;
    }
    if (virtual_path.size() >= 15 &&
        virtual_path.compare(virtual_path.size() - 15, 15, ".animation.yaml") ==
            0) {
      AnimationClipAssetDescriptor desc;
      if (AssetYaml::parseAnimationClipDescriptor(yaml_text, desc) &&
          matchesSource(desc.source)) {
        pushUnique(guid);
      }
    }
  }

  return result;
}

eastl::vector<eastl::string> guidsForDetectionWatchedPath(
    AssetWatchPathClass path_class, const fs::path& absolute_file_path,
    const fs::path& resources_root, const AssetRegistry& registry,
    FileSystem& file_system) {
  if (path_class == AssetWatchPathClass::SourceArchive) {
    // Sidecars under Source/ still expand to exchange/archive files.
    eastl::vector<eastl::string> result;
    auto pushUnique = [&result](const eastl::string& guid) {
      if (guid.empty()) {
        return;
      }
      for (const eastl::string& existing : result) {
        if (existing == guid) {
          return;
        }
      }
      result.push_back(guid);
    };
    const eastl::vector<fs::path> exchange_paths =
        resolveDetectionExchangePaths(absolute_file_path);
    if (exchange_paths.empty()) {
      return guidsForArchivedSourcePath(absolute_file_path, resources_root,
                                        registry, file_system);
    }
    for (const fs::path& exchange : exchange_paths) {
      const eastl::vector<eastl::string> mapped = guidsForArchivedSourcePath(
          exchange, resources_root, registry, file_system);
      for (const eastl::string& guid : mapped) {
        pushUnique(guid);
      }
    }
    // Also try the raw path (archived non-gltf sources).
    const eastl::vector<eastl::string> direct = guidsForArchivedSourcePath(
        absolute_file_path, resources_root, registry, file_system);
    for (const eastl::string& guid : direct) {
      pushUnique(guid);
    }
    return result;
  }

  if (path_class == AssetWatchPathClass::IntermediateResource) {
    return guidsForIntermediateSourcePath(absolute_file_path, resources_root,
                                          registry, file_system);
  }

  return {};
}

}  // namespace Blunder
