#include "runtime/resource/asset_import/gltf_animation_clip_extractor.h"

#include <cctype>
#include <cmath>
#include <cstring>

#include <cgltf.h>

#include "EASTL/hash_set.h"

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/resource/content_browser/content_browser_system.h"

#include <yaml-cpp/yaml.h>

namespace Blunder {

namespace fs = std::filesystem;

namespace {

eastl::string joinVirtualPath(const eastl::string& folder,
                              const eastl::string& file_name) {
  eastl::string result = folder;
  if (!result.empty() && result.back() != '/') {
    result.push_back('/');
  }
  result.append(file_name);
  return result;
}

bool isValidCharForClipName(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' ||
         c == '-';
}

void normalizeVirtualPathSlashes(eastl::string& path) {
  for (char& c : path) {
    if (c == '\\') {
      c = '/';
    }
  }
}

bool endsWithLiteral(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
}

eastl::string sanitizeClipName(const char* raw_name, size_t fallback_index) {
  eastl::string sanitized;
  if (raw_name != nullptr) {
    for (const char* cursor = raw_name; *cursor != '\0'; ++cursor) {
      const char c = *cursor;
      sanitized.push_back(isValidCharForClipName(c) ? c : '_');
    }
  }
  while (!sanitized.empty() && sanitized.front() == '_') {
    sanitized.erase(0, 1);
  }
  while (!sanitized.empty() && sanitized.back() == '_') {
    sanitized.pop_back();
  }
  if (sanitized.empty()) {
    char fallback[64];
    std::snprintf(fallback, sizeof(fallback), "animation_%zu", fallback_index);
    return eastl::string(fallback);
  }
  return sanitized;
}

bool mapChannel(cgltf_animation_path_type path, AnimationChannel& out_channel) {
  switch (path) {
    case cgltf_animation_path_type_translation:
      out_channel = AnimationChannel::Translation;
      return true;
    case cgltf_animation_path_type_rotation:
      out_channel = AnimationChannel::Rotation;
      return true;
    case cgltf_animation_path_type_scale:
      out_channel = AnimationChannel::Scale;
      return true;
    default:
      return false;
  }
}

bool mapInterpolation(cgltf_interpolation_type interpolation,
                      AnimationInterpolation& out_interpolation) {
  switch (interpolation) {
    case cgltf_interpolation_type_step:
      out_interpolation = AnimationInterpolation::Constant;
      return true;
    case cgltf_interpolation_type_linear:
      out_interpolation = AnimationInterpolation::Linear;
      return true;
    default:
      return false;
  }
}

size_t valueComponentCount(AnimationChannel channel) {
  return channel == AnimationChannel::Rotation ? 4u : 3u;
}

eastl::string boneNameForNode(const cgltf_node* node) {
  if (node == nullptr) {
    return eastl::string("Node");
  }
  if (node->name != nullptr && node->name[0] != '\0') {
    return eastl::string(node->name);
  }
  return eastl::string("Node");
}

bool readAccessorFloats(const cgltf_accessor* accessor, cgltf_size index,
                        eastl::vector<float>& out_values) {
  if (accessor == nullptr) {
    return false;
  }
  const cgltf_size component_count =
      accessor->type == cgltf_type_scalar
          ? 1
          : (accessor->type == cgltf_type_vec3
                 ? 3
                 : (accessor->type == cgltf_type_vec4 ? 4 : 0));
  if (component_count == 0) {
    return false;
  }
  out_values.resize(static_cast<size_t>(component_count));
  cgltf_float buffer[4]{};
  if (!cgltf_accessor_read_float(accessor, index, buffer, component_count)) {
    return false;
  }
  for (cgltf_size i = 0; i < component_count; ++i) {
    out_values[static_cast<size_t>(i)] = static_cast<float>(buffer[i]);
  }
  return true;
}

bool buildTrackFromChannel(const cgltf_animation_channel& channel,
                           AnimationTrack& out_track, float& out_max_time) {
  if (channel.sampler == nullptr || channel.target_node == nullptr) {
    return false;
  }
  if (channel.sampler->interpolation == cgltf_interpolation_type_cubic_spline) {
    LOG_WARN(
        "[AssetImport] skipping cubic spline animation channel on bone {}",
        boneNameForNode(channel.target_node).c_str());
    return false;
  }
  if (!mapChannel(channel.target_path, out_track.channel)) {
    if (channel.target_path == cgltf_animation_path_type_weights) {
      LOG_WARN("[AssetImport] skipping morph weights animation channel");
    }
    return false;
  }
  if (!mapInterpolation(channel.sampler->interpolation,
                        out_track.interpolation)) {
    return false;
  }

  const cgltf_accessor* input = channel.sampler->input;
  const cgltf_accessor* output = channel.sampler->output;
  if (input == nullptr || output == nullptr) {
    return false;
  }
  if (input->count != output->count) {
    LOG_WARN("[AssetImport] animation channel key count mismatch");
    return false;
  }

  out_track.bone = boneNameForNode(channel.target_node);
  const size_t components = valueComponentCount(out_track.channel);
  out_track.keys.clear();
  out_track.keys.reserve(static_cast<size_t>(input->count));

  for (cgltf_size key_index = 0; key_index < input->count; ++key_index) {
    eastl::vector<float> time_value;
    if (!readAccessorFloats(input, key_index, time_value) ||
        time_value.size() != 1) {
      return false;
    }

    eastl::vector<float> raw_value;
    if (!readAccessorFloats(output, key_index, raw_value)) {
      return false;
    }
    if (raw_value.size() != components) {
      return false;
    }

    AnimationKeyframe keyframe;
    keyframe.time = time_value[0];
    keyframe.value = raw_value;
    out_track.keys.push_back(keyframe);
    if (keyframe.time > out_max_time) {
      out_max_time = keyframe.time;
    }
  }
  return !out_track.keys.empty();
}

bool buildClipData(const cgltf_animation& animation, AnimationClipData& out_data) {
  out_data = {};
  out_data.name = animation.name != nullptr ? eastl::string(animation.name)
                                            : eastl::string();
  float max_time = 0.0f;

  for (cgltf_size channel_index = 0; channel_index < animation.channels_count;
       ++channel_index) {
    AnimationTrack track;
    if (!buildTrackFromChannel(animation.channels[channel_index], track,
                               max_time)) {
      continue;
    }
    out_data.tracks.push_back(track);
  }

  if (animation.extras.data != nullptr && animation.extras.data[0] != '\0') {
    try {
      const YAML::Node extras = YAML::Load(animation.extras.data);
      const YAML::Node method_keys = extras["method_keys"];
      if (method_keys && method_keys.IsSequence()) {
        for (const auto& key_node : method_keys) {
          if (!key_node || !key_node.IsMap()) {
            continue;
          }
          AnimationMethodKey key;
          const YAML::Node name_node = key_node["name"];
          const YAML::Node time_node = key_node["time"];
          if (!name_node || !name_node.IsScalar() || !time_node ||
              !time_node.IsScalar()) {
            continue;
          }
          key.name = name_node.as<std::string>().c_str();
          key.time = time_node.as<float>();
          const YAML::Node args_node = key_node["args"];
          if (args_node && args_node.IsSequence()) {
            for (const auto& arg_node : args_node) {
              if (arg_node && arg_node.IsScalar()) {
                key.args.push_back(arg_node.as<float>());
              }
            }
          }
          if (!key.name.empty()) {
            out_data.method_keys.push_back(key);
            if (key.time > max_time) {
              max_time = key.time;
            }
          }
        }
      }
    } catch (const YAML::Exception& exception) {
      LOG_WARN("[AssetImport] failed to parse animation extras method_keys: {}",
               exception.what());
    }
  }

  out_data.duration = max_time;
  return !out_data.tracks.empty() || !out_data.method_keys.empty();
}

bool loadGltfDocument(FileSystem* file_system,
                      const fs::path& gltf_absolute, cgltf_data** out_data) {
  *out_data = nullptr;
  if (file_system == nullptr) {
    return false;
  }

  eastl::vector<uint8_t> bytes;
  if (!file_system->readBinary(gltf_absolute, bytes) || bytes.empty()) {
    LOG_WARN("[AssetImport] failed to read glTF for animation extract {}",
             gltf_absolute.generic_string());
    return false;
  }

  cgltf_options options{};
  cgltf_data* data = nullptr;
  const cgltf_result parse_result =
      cgltf_parse(&options, bytes.data(), bytes.size(), &data);
  if (parse_result != cgltf_result_success || data == nullptr) {
    LOG_WARN("[AssetImport] cgltf_parse failed for {} ({})",
             gltf_absolute.generic_string(), static_cast<int>(parse_result));
    return false;
  }

  const std::string absolute_string = gltf_absolute.string();
  const cgltf_result buffer_result =
      cgltf_load_buffers(&options, data, absolute_string.c_str());
  if (buffer_result != cgltf_result_success) {
    cgltf_free(data);
    LOG_WARN("[AssetImport] cgltf_load_buffers failed for {} ({})",
             gltf_absolute.generic_string(), static_cast<int>(buffer_result));
    return false;
  }

  *out_data = data;
  return true;
}

bool intermediateClipExists(FileSystem* file_system,
                            ContentBrowserSystem* content_browser,
                            const eastl::string& virtual_path) {
  if (content_browser && content_browser->findEntry(virtual_path)) {
    return true;
  }
  eastl::string relative = virtual_path;
  if (relative.compare(0, 10, "resources/") == 0) {
    relative.erase(0, 10);
  }
  const fs::path absolute =
      file_system->resolveResource(fs::path(relative.c_str()));
  return file_system->exists(absolute);
}

eastl::string makeUniqueIntermediateClipVirtualPath(
    FileSystem* file_system, ContentBrowserSystem* content_browser,
    const eastl::string& mesh_stem, const eastl::string& clip_stem) {
  auto try_name = [&](const eastl::string& folder_stem,
                      const eastl::string& file_stem) -> eastl::string {
    eastl::string file_name = file_stem;
    file_name.append(".anim.yaml");
    const fs::path relative = fs::path("Animations") / folder_stem.c_str() /
                              file_name.c_str();
    eastl::string virtual_path("resources/");
    virtual_path.append(relative.generic_string().c_str());
    normalizeVirtualPathSlashes(virtual_path);
    if (intermediateClipExists(file_system, content_browser, virtual_path)) {
      return eastl::string();
    }
    return virtual_path;
  };

  eastl::string virtual_path = try_name(mesh_stem, clip_stem);
  if (!virtual_path.empty()) {
    return virtual_path;
  }

  for (uint32_t index = 1; index < 10000; ++index) {
    char alt[128];
    std::snprintf(alt, sizeof(alt), "%s_%u", clip_stem.c_str(), index);
    virtual_path = try_name(mesh_stem, eastl::string(alt));
    if (!virtual_path.empty()) {
      return virtual_path;
    }
  }
  return eastl::string();
}

fs::path resolveResourcesVirtualPath(FileSystem* file_system,
                                     const eastl::string& virtual_path) {
  eastl::string relative = virtual_path;
  if (relative.compare(0, 10, "resources/") == 0) {
    relative.erase(0, 10);
  }
  return file_system->resolveResource(fs::path(relative.c_str()));
}

fs::path resolveDescriptorAbsolute(FileSystem* file_system,
                                   const eastl::string& descriptor_virtual) {
  eastl::string relative = descriptor_virtual;
  if (relative.compare(0, 7, "assets/") == 0) {
    relative.erase(0, 7);
  }
  return file_system->resolveAsset(fs::path(relative.c_str()));
}

ImportResult registerSingleAnimationClip(
    FileSystem* file_system, AssetRegistry* asset_registry,
    ContentBrowserSystem* content_browser, const eastl::string& mesh_stem,
    const eastl::string& clip_stem, const AnimationClipData& clip_data,
    const MakeUniqueDescriptorNameFn& make_unique_descriptor_name,
    const eastl::string& assets_folder_virtual,
    const ExistingAnimationClipBinding* existing_binding = nullptr) {
  ImportResult result{};
  if (file_system == nullptr || asset_registry == nullptr ||
      !make_unique_descriptor_name) {
    return result;
  }

  eastl::string intermediate_virtual;
  if (existing_binding != nullptr) {
    intermediate_virtual = existing_binding->intermediate_virtual;
  } else {
    intermediate_virtual = makeUniqueIntermediateClipVirtualPath(
        file_system, content_browser, mesh_stem, clip_stem);
    if (intermediate_virtual.empty()) {
      LOG_WARN("[AssetImport] failed to allocate Intermediate clip path for {}",
               clip_stem.c_str());
      return result;
    }
  }

  const fs::path intermediate_absolute =
      resolveResourcesVirtualPath(file_system, intermediate_virtual);
  file_system->ensureParentDirectory(intermediate_absolute);
  if (!file_system->writeText(intermediate_absolute,
                              AssetYaml::serializeAnimationClipData(clip_data))) {
    LOG_WARN("[AssetImport] failed to write clip Intermediate {}",
             intermediate_absolute.generic_string());
    return result;
  }

  if (existing_binding != nullptr) {
    result.descriptor_virtual_path = existing_binding->descriptor_virtual;
    result.guid = existing_binding->guid;
    result.success = true;
    LOG_INFO("[AssetImport] animation clip refresh {} -> {} (Intermediate: {})",
             clip_stem.c_str(), result.descriptor_virtual_path.c_str(),
             intermediate_virtual.c_str());
    return result;
  }

  eastl::string folder = assets_folder_virtual.empty() ? eastl::string("assets/Animations")
                                                       : assets_folder_virtual;
  while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\')) {
    folder.pop_back();
  }
  if (folder.empty()) {
    folder = "assets/Animations";
  }

  const eastl::string descriptor_name =
      make_unique_descriptor_name(folder, clip_stem, ".animation.yaml");
  if (descriptor_name.empty()) {
    LOG_WARN("[AssetImport] descriptor already exists for clip {}",
             clip_stem.c_str());
    return result;
  }

  AnimationClipAssetDescriptor descriptor{};
  descriptor.guid = asset_registry->allocateGuid();
  descriptor.source = intermediate_virtual;
  normalizeVirtualPathSlashes(descriptor.source);

  const eastl::string descriptor_virtual =
      joinVirtualPath(folder, descriptor_name);
  eastl::string relative = descriptor_virtual;
  relative.erase(0, 7);
  const fs::path descriptor_absolute =
      file_system->resolveAsset(fs::path(relative.c_str()));

  file_system->ensureParentDirectory(descriptor_absolute);
  if (!file_system->writeText(
          descriptor_absolute,
          AssetYaml::serializeAnimationClipDescriptor(descriptor))) {
    return result;
  }

  asset_registry->registerAsset(descriptor.guid, descriptor_virtual);

  result.descriptor_virtual_path = descriptor_virtual;
  result.guid = descriptor.guid;
  result.success = true;

  LOG_INFO("[AssetImport] animation clip {} -> {} (Intermediate: {})",
           clip_stem.c_str(), descriptor_virtual.c_str(),
           intermediate_virtual.c_str());
  return result;
}

eastl::string descriptorStemBeforeSuffix(const eastl::string& descriptor_virtual,
                                         const char* suffix) {
  size_t slash = eastl::string::npos;
  for (size_t i = descriptor_virtual.size(); i > 0; --i) {
    const char c = descriptor_virtual[i - 1];
    if (c == '/' || c == '\\') {
      slash = i - 1;
      break;
    }
  }
  const size_t name_start = slash == eastl::string::npos ? 0 : slash + 1;
  eastl::string filename = descriptor_virtual.substr(name_start);
  const size_t suffix_length = std::strlen(suffix);
  if (filename.size() >= suffix_length &&
      filename.compare(filename.size() - suffix_length, suffix_length, suffix) ==
          0) {
    filename.erase(filename.size() - suffix_length);
  }
  return filename;
}

eastl::vector<ImportResult> processAnimationClipsFromGltf(
    FileSystem* file_system, AssetRegistry* asset_registry,
    ContentBrowserSystem* content_browser,
    const fs::path& gltf_absolute, const eastl::string& mesh_stem,
    const MakeUniqueDescriptorNameFn& make_unique_descriptor_name,
    const ExistingAnimationClipMap* existing_clips,
    const eastl::string& preferred_clip_stem,
    const eastl::string& assets_folder_virtual) {
  eastl::vector<ImportResult> results;
  if (file_system == nullptr || asset_registry == nullptr ||
      !make_unique_descriptor_name) {
    return results;
  }

  cgltf_data* data = nullptr;
  if (!loadGltfDocument(file_system, gltf_absolute, &data) || data == nullptr) {
    return results;
  }

  eastl::hash_set<eastl::string> matched_guids;
  for (cgltf_size animation_index = 0; animation_index < data->animations_count;
       ++animation_index) {
    const cgltf_animation& animation = data->animations[animation_index];
    AnimationClipData clip_data;
    if (!buildClipData(animation, clip_data)) {
      continue;
    }

    if (clip_data.name.empty()) {
      clip_data.name =
          sanitizeClipName(nullptr, static_cast<size_t>(animation_index));
    }

    eastl::string clip_stem;
    if (!preferred_clip_stem.empty()) {
      clip_stem = sanitizeClipName(preferred_clip_stem.c_str(), 0);
      if (animation_index > 0) {
        char suffix[32];
        std::snprintf(suffix, sizeof(suffix), "_%zu",
                      static_cast<size_t>(animation_index));
        clip_stem.append(suffix);
      }
    } else {
      clip_stem = sanitizeClipName(clip_data.name.c_str(),
                                   static_cast<size_t>(animation_index));
    }
    clip_data.name = clip_stem;

    const ExistingAnimationClipBinding* existing_binding = nullptr;
    if (existing_clips != nullptr) {
      const auto it = existing_clips->find(clip_stem);
      if (it != existing_clips->end()) {
        existing_binding = &it->second;
        matched_guids.insert(existing_binding->guid);
      }
    }

    ImportResult imported = registerSingleAnimationClip(
        file_system, asset_registry, content_browser, mesh_stem, clip_stem,
        clip_data, make_unique_descriptor_name, assets_folder_virtual,
        existing_binding);
    if (imported.success) {
      results.push_back(imported);
    }
  }

  if (existing_clips != nullptr) {
    eastl::hash_set<eastl::string> logged_orphan_guids;
    for (const auto& entry : *existing_clips) {
      const ExistingAnimationClipBinding& binding = entry.second;
      if (matched_guids.find(binding.guid) != matched_guids.end()) {
        continue;
      }
      if (logged_orphan_guids.find(binding.guid) != logged_orphan_guids.end()) {
        continue;
      }
      logged_orphan_guids.insert(binding.guid);
      LOG_INFO(
          "[AssetImport] animation clip orphan left in place: {} ({})",
          binding.descriptor_virtual.c_str(), binding.guid.c_str());
    }
  }

  cgltf_free(data);
  return results;
}

}  // namespace

ExistingAnimationClipMap collectExistingAnimationClipsForMesh(
    FileSystem* file_system, AssetRegistry* asset_registry,
    const eastl::string& mesh_stem) {
  ExistingAnimationClipMap result;
  if (file_system == nullptr || mesh_stem.empty()) {
    return result;
  }

  eastl::string expected_prefix("resources/Animations/");
  expected_prefix.append(mesh_stem);
  expected_prefix.push_back('/');

  const fs::path asset_root = file_system->getAssetRoot();
  if (!file_system->isDirectory(asset_root)) {
    return result;
  }

  const eastl::vector<DirectoryEntry> entries =
      file_system->listDirectoryRecursive(asset_root, asset_root, -1);
  for (const DirectoryEntry& entry : entries) {
    if (entry.is_directory) {
      continue;
    }

    eastl::string relative_path = entry.relative_path;
    for (char& c : relative_path) {
      if (c == '\\') {
        c = '/';
      }
    }
    const eastl::string descriptor_virtual =
        eastl::string("assets/") + relative_path;
    if (!endsWithLiteral(descriptor_virtual, ".animation.yaml")) {
      continue;
    }

    eastl::string descriptor_yaml;
    if (!file_system->readText(entry.absolute_path, descriptor_yaml)) {
      continue;
    }

    AnimationClipAssetDescriptor descriptor{};
    if (!AssetYaml::parseAnimationClipDescriptor(descriptor_yaml, descriptor)) {
      continue;
    }
    normalizeVirtualPathSlashes(descriptor.source);
    if (descriptor.source.compare(0, expected_prefix.size(), expected_prefix) !=
        0) {
      continue;
    }

    eastl::string guid = descriptor.guid;
    if (guid.empty() && asset_registry != nullptr) {
      guid = asset_registry->findGuidForPath(descriptor_virtual);
    }
    if (guid.empty()) {
      continue;
    }

    const eastl::string descriptor_stem =
        descriptorStemBeforeSuffix(descriptor_virtual, ".animation.yaml");

    eastl::string clip_name = descriptor_stem;
    const fs::path intermediate_absolute =
        resolveResourcesVirtualPath(file_system, descriptor.source);
    eastl::string intermediate_yaml;
    if (file_system->readText(intermediate_absolute, intermediate_yaml)) {
      AnimationClipData clip_data{};
      if (AssetYaml::parseAnimationClipData(intermediate_yaml, clip_data) &&
          !clip_data.name.empty()) {
        clip_name = clip_data.name;
      }
    }

    ExistingAnimationClipBinding binding{};
    binding.guid = guid;
    binding.intermediate_virtual = descriptor.source;
    binding.descriptor_virtual = descriptor_virtual;
    result[clip_name] = binding;
    if (descriptor_stem != clip_name) {
      result[descriptor_stem] = binding;
    }
  }

  return result;
}

eastl::vector<ImportResult> extractAndRegisterAnimationClipsFromGltf(
    FileSystem* file_system, AssetRegistry* asset_registry,
    ContentBrowserSystem* content_browser,
    const fs::path& gltf_absolute, const eastl::string& mesh_stem,
    const MakeUniqueDescriptorNameFn& make_unique_descriptor_name,
    const eastl::string& preferred_clip_stem,
    const eastl::string& assets_folder_virtual) {
  return processAnimationClipsFromGltf(
      file_system, asset_registry, content_browser, gltf_absolute, mesh_stem,
      make_unique_descriptor_name, nullptr, preferred_clip_stem,
      assets_folder_virtual);
}

eastl::vector<ImportResult> refreshAnimationClipsFromGltf(
    FileSystem* file_system, AssetRegistry* asset_registry,
    ContentBrowserSystem* content_browser,
    const fs::path& gltf_absolute, const eastl::string& mesh_stem,
    const ExistingAnimationClipMap& existing_clips,
    const MakeUniqueDescriptorNameFn& make_unique_descriptor_name,
    const eastl::string& preferred_clip_stem,
    const eastl::string& assets_folder_virtual) {
  return processAnimationClipsFromGltf(
      file_system, asset_registry, content_browser, gltf_absolute, mesh_stem,
      make_unique_descriptor_name, &existing_clips, preferred_clip_stem,
      assets_folder_virtual);
}

}  // namespace Blunder
