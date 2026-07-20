#include "runtime/resource/asset_cook/asset_watch_path.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

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

}  // namespace Blunder
