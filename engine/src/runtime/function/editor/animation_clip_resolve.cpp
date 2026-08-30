#include "runtime/function/editor/animation_clip_resolve.h"

#include "runtime/core/math/coordinate_system.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

#include <filesystem>

namespace Blunder {

namespace {

std::filesystem::path resolveVirtualFile(FileSystem& file_system,
                                         const eastl::string& virtual_path) {
  if (virtual_path.compare(0, 7, "assets/") == 0) {
    return file_system.resolveAsset(
        std::filesystem::path(virtual_path.c_str() + 7));
  }
  if (virtual_path.compare(0, 10, "resources/") == 0) {
    return file_system.resolveResource(
        std::filesystem::path(virtual_path.c_str() + 10));
  }
  return file_system.resolveAsset(std::filesystem::path(virtual_path.c_str()));
}

bool readTextAsset(const eastl::string& virtual_path, eastl::string& out_text) {
  FileSystem* file_system = g_runtime_global_context.m_file_system.get();
  if (file_system == nullptr || virtual_path.empty()) {
    return false;
  }
  return file_system->readText(resolveVirtualFile(*file_system, virtual_path),
                               out_text);
}

void convertClipTracksGltfToEngine(AnimationClipData& clip) {
  for (AnimationTrack& track : clip.tracks) {
    for (AnimationKeyframe& key : track.keys) {
      if (track.channel == AnimationChannel::Translation &&
          key.value.size() >= 3) {
        const Vec3 engine = transformPointGltfToEngine(
            Vec3(key.value[0], key.value[1], key.value[2]));
        key.value[0] = engine.x;
        key.value[1] = engine.y;
        key.value[2] = engine.z;
      } else if (track.channel == AnimationChannel::Rotation &&
                 key.value.size() >= 4) {
        const Quat engine = transformRotationGltfToEngine(
            Quat(key.value[3], key.value[0], key.value[1], key.value[2]));
        key.value[0] = engine.x;
        key.value[1] = engine.y;
        key.value[2] = engine.z;
        key.value[3] = engine.w;
      } else if (track.channel == AnimationChannel::Scale &&
                 key.value.size() >= 3) {
        const Vec3 engine = transformScaleGltfToEngine(
            Vec3(key.value[0], key.value[1], key.value[2]));
        key.value[0] = engine.x;
        key.value[1] = engine.y;
        key.value[2] = engine.z;
      }
    }
  }
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

  const bool parsed = AssetYaml::parseAnimationClipData(clip_yaml, out_clip);
  if (parsed) {
    convertClipTracksGltfToEngine(out_clip);
  }
  return parsed;
}

void wireAnimationPlayerAssetResolver(AnimationPlayer& player) {
  player.setClipResolver(resolveAnimationClipFromAssets, nullptr);
}

}  // namespace Blunder
