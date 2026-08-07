#include "runtime/resource/thumbnail/scene_thumbnail_fingerprint.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/guid.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_serializer.h"

namespace Blunder {
namespace {

eastl::string stripAssetsPrefix(eastl::string path) {
  if (path.compare(0, 7, "assets/") == 0) {
    path.erase(0, 7);
  }
  return path;
}

bool readSceneAssetText(FileSystem& file_system,
                        const eastl::string& scene_virtual_path,
                        eastl::string& out_text) {
  const eastl::string relative = stripAssetsPrefix(scene_virtual_path);
  const std::filesystem::path absolute =
      file_system.resolveAsset(std::filesystem::path(relative.c_str()));
  return file_system.readText(absolute, out_text);
}

uint64_t fileMtimeOrZero(FileSystem& file_system,
                         const std::filesystem::path& absolute) {
  std::error_code ec;
  const auto stamp = std::filesystem::last_write_time(absolute, ec);
  if (ec) {
    return 0;
  }
  return static_cast<uint64_t>(
      stamp.time_since_epoch().count());
}

uint64_t meshRefMtime(FileSystem& file_system, AssetRegistry* asset_registry,
                      const eastl::string& mesh_ref) {
  eastl::string path = mesh_ref;
  if (isValidGuidFormat(mesh_ref) && asset_registry != nullptr) {
    const eastl::string resolved = asset_registry->resolveGuid(mesh_ref);
    if (!resolved.empty()) {
      path = resolved;
    }
  }
  if (path.empty()) {
    return 0;
  }
  const eastl::string relative = stripAssetsPrefix(path);
  const std::filesystem::path absolute =
      file_system.resolveAsset(std::filesystem::path(relative.c_str()));
  return fileMtimeOrZero(file_system, absolute);
}

void collectRecursive(FileSystem& file_system, AssetRegistry* asset_registry,
                      const eastl::string& scene_virtual_path,
                      eastl::vector<eastl::string>& out_refs,
                      eastl::vector<eastl::string>& visited) {
  for (const eastl::string& seen : visited) {
    if (seen == scene_virtual_path) {
      return;
    }
  }
  visited.push_back(scene_virtual_path);

  eastl::string json_text;
  if (!readSceneAssetText(file_system, scene_virtual_path, json_text)) {
    return;
  }

  Scene scene;
  if (!SceneSerializer::deserialize(json_text, scene, asset_registry)) {
    return;
  }

  for (const SceneEntityDefinition& entity : scene.getEntities()) {
    if (!entity.mesh_virtual_path.empty()) {
      out_refs.push_back(entity.mesh_virtual_path);
    }
  }
  for (const SceneChildReference& child : scene.getChildScenes()) {
    if (!child.scene_virtual_path.empty()) {
      collectRecursive(file_system, asset_registry, child.scene_virtual_path,
                       out_refs, visited);
    }
  }
}

}  // namespace

eastl::vector<eastl::string> collectSceneDirectMeshReferences(
    FileSystem& file_system, AssetRegistry* asset_registry,
    const eastl::string& scene_virtual_path) {
  eastl::vector<eastl::string> refs;
  eastl::vector<eastl::string> visited;
  collectRecursive(file_system, asset_registry, scene_virtual_path, refs,
                   visited);
  std::sort(refs.begin(), refs.end());
  refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
  return refs;
}

eastl::string computeSceneThumbnailFingerprint(
    FileSystem& file_system, AssetRegistry* asset_registry,
    const eastl::string& scene_virtual_path, uint64_t scene_mtime) {
  const eastl::vector<eastl::string> refs = collectSceneDirectMeshReferences(
      file_system, asset_registry, scene_virtual_path);

  eastl::string out;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "sc:%llx",
                static_cast<unsigned long long>(scene_mtime));
  out.append(buf);

  for (const eastl::string& ref : refs) {
    const uint64_t mtime = meshRefMtime(file_system, asset_registry, ref);
    out.append("|");
    out.append(ref);
    std::snprintf(buf, sizeof(buf), "@%llx",
                  static_cast<unsigned long long>(mtime));
    out.append(buf);
  }
  return out;
}

}  // namespace Blunder
