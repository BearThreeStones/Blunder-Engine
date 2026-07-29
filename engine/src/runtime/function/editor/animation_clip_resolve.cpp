#include "runtime/function/editor/animation_clip_resolve.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

namespace {

eastl::string stripAssetsPrefix(eastl::string path) {
  if (path.compare(0, 7, "assets/") == 0) {
    path.erase(0, 7);
  }
  return path;
}

bool readTextAsset(const eastl::string& virtual_path, eastl::string& out_text) {
  FileSystem* file_system = g_runtime_global_context.m_file_system.get();
  if (file_system == nullptr || virtual_path.empty()) {
    return false;
  }
  const eastl::string relative = stripAssetsPrefix(virtual_path);
  const std::filesystem::path absolute =
      file_system->resolveAsset(std::filesystem::path(relative.c_str()));
  return file_system->readText(absolute, out_text);
}

}  // namespace

bool resolveAnimationClipFromAssets(void* /*userdata*/, const eastl::string& guid,
                                    AnimationClipData& out_clip) {
  AssetManager* asset_manager = g_runtime_global_context.m_asset_manager.get();
  AssetRegistry* asset_registry = g_runtime_global_context.m_asset_registry.get();
  if (asset_manager == nullptr || asset_registry == nullptr || guid.empty()) {
    return false;
  }

  const eastl::string descriptor_virtual =
      asset_manager->resolveGuidPath(guid, *asset_registry);
  if (descriptor_virtual.empty()) {
    return false;
  }

  eastl::string descriptor_yaml;
  if (!readTextAsset(descriptor_virtual, descriptor_yaml)) {
    return false;
  }

  AnimationClipAssetDescriptor descriptor{};
  if (!AssetYaml::parseAnimationClipDescriptor(descriptor_yaml, descriptor)) {
    return false;
  }
  if (descriptor.source.empty()) {
    return false;
  }

  eastl::string clip_yaml;
  if (!readTextAsset(descriptor.source, clip_yaml)) {
    return false;
  }

  return AssetYaml::parseAnimationClipData(clip_yaml, out_clip);
}

void wireAnimationPlayerAssetResolver(AnimationPlayer& player) {
  player.setClipResolver(resolveAnimationClipFromAssets, nullptr);
}

}  // namespace Blunder
