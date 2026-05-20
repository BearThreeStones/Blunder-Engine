#include "runtime/resource/asset_manager/asset_manager.h"

#include "runtime/function/scene/scene_serializer.h"
#include "runtime/resource/asset/scene_asset.h"

#include <cgltf.h>
#include <spdlog/fmt/fmt.h>
#include <stb_image.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>

#include <algorithm>

#include "EASTL/utility.h"
#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"

namespace Blunder {

namespace {
uint64_t querySourceTimestamp(const std::filesystem::path& path) {
  std::error_code ec;
  const auto t = std::filesystem::last_write_time(path, ec);
  if (ec) {
    return 0;
  }
  return static_cast<uint64_t>(t.time_since_epoch().count());
}

const char* cgltfResultToString(cgltf_result result) {
  switch (result) {
    case cgltf_result_success:
      return "success";
    case cgltf_result_data_too_short:
      return "data_too_short";
    case cgltf_result_unknown_format:
      return "unknown_format";
    case cgltf_result_invalid_json:
      return "invalid_json";
    case cgltf_result_invalid_gltf:
      return "invalid_gltf";
    case cgltf_result_invalid_options:
      return "invalid_options";
    case cgltf_result_file_not_found:
      return "file_not_found";
    case cgltf_result_io_error:
      return "io_error";
    case cgltf_result_out_of_memory:
      return "out_of_memory";
    case cgltf_result_legacy_gltf:
      return "legacy_gltf";
    default:
      return "unknown";
  }
}

bool relativePathEscapesRoot(const std::filesystem::path& relative_path) {
  for (const auto& part : relative_path.lexically_normal()) {
    if (part == ".") {
      continue;
    }
    return part == "..";
  }
  return false;
}

eastl::string buildGeneratedKey(const eastl::string& base_key, const char* kind,
                                size_t index) {
  char suffix[64];
  std::snprintf(suffix, sizeof(suffix), ".generated.%s.%zu", kind, index);

  eastl::string generated_key(base_key);
  generated_key.append(suffix);
  return generated_key;
}

bool startsWithPrefix(const eastl::string& path, const char* prefix) {
  const size_t prefix_length = std::strlen(prefix);
  if (path.size() < prefix_length) {
    return false;
  }
  return path.compare(0, prefix_length, prefix) == 0;
}

eastl::string stripPrefix(const eastl::string& path, const char* prefix) {
  const size_t prefix_length = std::strlen(prefix);
  if (startsWithPrefix(path, prefix)) {
    return eastl::string(path.c_str() + prefix_length);
  }
  return path;
}

eastl::string buildResourceVirtualPath(
    const std::filesystem::path& resources_root,
    const std::filesystem::path& source_path, const char* uri) {
  const std::filesystem::path candidate = std::filesystem::path(uri).is_absolute()
                                              ? std::filesystem::path(uri)
                                              : (source_path.parent_path() / uri);
  const std::filesystem::path normalized = candidate.lexically_normal();

  std::error_code ec;
  const std::filesystem::path relative =
      std::filesystem::relative(normalized, resources_root, ec);
  if (!ec && !relative.empty() && !relativePathEscapesRoot(relative)) {
    eastl::string virtual_path("resources/");
    virtual_path.append(relative.generic_string().c_str());
    return virtual_path;
  }

  return eastl::string(normalized.generic_string().c_str());
}

struct ResolvedContentPath {
  eastl::string canonical_key;
  std::filesystem::path absolute;
};

ResolvedContentPath resolveContentPath(FileSystem* file_system,
                                       const eastl::string& virtual_path,
                                       bool default_to_resources) {
  const eastl::string normalized_key =
      eastl::string(std::filesystem::path(virtual_path.c_str())
                        .lexically_normal()
                        .generic_string()
                        .c_str());

  ResolvedContentPath resolved{};
  resolved.canonical_key = normalized_key;

  if (startsWithPrefix(normalized_key, "resources/")) {
    const eastl::string relative = stripPrefix(normalized_key, "resources/");
    resolved.absolute = file_system->resolveResource(
        std::filesystem::path(relative.c_str()));
    return resolved;
  }

  if (startsWithPrefix(normalized_key, "assets/")) {
    const eastl::string relative = stripPrefix(normalized_key, "assets/");
    resolved.absolute =
        file_system->resolveAsset(std::filesystem::path(relative.c_str()));
    return resolved;
  }

  if (default_to_resources) {
    resolved.canonical_key = eastl::string("resources/");
    resolved.canonical_key.append(normalized_key);
    resolved.absolute = file_system->resolveResource(
        std::filesystem::path(normalized_key.c_str()));
  } else {
    resolved.canonical_key = eastl::string("assets/");
    resolved.canonical_key.append(normalized_key);
    resolved.absolute = file_system->resolveAsset(
        std::filesystem::path(normalized_key.c_str()));
  }
  return resolved;
}

bool parseMeshAssetSource(const eastl::string& json_text,
                          eastl::string& out_source) {
  const char* key = "\"source\"";
  const char* key_pos = std::strstr(json_text.c_str(), key);
  if (key_pos == nullptr) {
    return false;
  }

  const char* colon = std::strchr(key_pos + std::strlen(key), ':');
  if (colon == nullptr) {
    return false;
  }

  const char* quote_start = std::strchr(colon, '"');
  if (quote_start == nullptr) {
    return false;
  }
  ++quote_start;

  const char* quote_end = std::strchr(quote_start, '"');
  if (quote_end == nullptr || quote_end == quote_start) {
    return false;
  }

  out_source.assign(quote_start, static_cast<size_t>(quote_end - quote_start));
  return !out_source.empty();
}

bool isDataUri(const char* uri) {
  return uri != nullptr && std::strncmp(uri, "data:", 5) == 0;
}

const cgltf_attribute* findPrimitiveAttribute(const cgltf_primitive& primitive,
                                              cgltf_attribute_type type,
                                              cgltf_int index = 0) {
  for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
    const cgltf_attribute& attribute = primitive.attributes[i];
    if (attribute.type == type && attribute.index == index) {
      return &attribute;
    }
  }
  return nullptr;
}
}  // namespace

void AssetManager::initialize(const AssetManagerInitInfo& info) {
  if (m_is_initialized) {
    return;
  }

  if (info.file_system == nullptr) {
    LOG_FATAL("[AssetManager] initialize() requires a non-null FileSystem");
  }

  m_file_system = info.file_system;
  m_is_initialized = true;
  LOG_INFO(
      "[AssetManager] initialized (assets: {}, resources: {})",
      m_file_system->getAssetRoot().generic_string(),
      m_file_system->getResourcesRoot().generic_string());
}

void AssetManager::shutdown() {
  if (!m_is_initialized) {
    return;
  }
  clearCache();
  m_file_system = nullptr;
  m_is_initialized = false;
}

eastl::shared_ptr<Texture2DAsset> AssetManager::loadTexture2D(
    const eastl::string& virtual_path) {
  if (!m_is_initialized) {
    LOG_ERROR("[AssetManager] loadTexture2D before initialize()");
    return nullptr;
  }
  if (virtual_path.empty()) {
    LOG_ERROR("[AssetManager] loadTexture2D: empty path");
    return nullptr;
  }

  const ResolvedContentPath resolved =
      resolveContentPath(m_file_system, virtual_path, true);
  const eastl::string& key = resolved.canonical_key;
  if (auto it = m_texture_cache.find(key);
      it != m_texture_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_texture_cache.erase(it);
  }

  const auto& absolute = resolved.absolute;

  eastl::vector<uint8_t> bytes;
  if (!m_file_system->readBinary(absolute, bytes)) {
    LOG_ERROR("[AssetManager] loadTexture2D: cannot read {}",
              absolute.generic_string());
    return nullptr;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  // Always decode to 4 channels (RGBA8) so the GPU upload path stays uniform.
  stbi_uc* decoded = stbi_load_from_memory(
      bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels,
      STBI_rgb_alpha);

  if (decoded == nullptr) {
    LOG_ERROR("[AssetManager] loadTexture2D: stb_image failed for {} ({})",
              absolute.generic_string(),
              stbi_failure_reason() ? stbi_failure_reason() : "unknown");
    return nullptr;
  }

  const size_t pixel_byte_size =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  eastl::vector<uint8_t> pixels(pixel_byte_size);
  std::memcpy(pixels.data(), decoded, pixel_byte_size);
  stbi_image_free(decoded);

  Asset::Meta meta;
  meta.virtual_path = key;
  meta.absolute_path = absolute;
  meta.source_timestamp = querySourceTimestamp(absolute);
  auto asset = eastl::make_shared<Texture2DAsset>(
      eastl::move(meta), static_cast<uint32_t>(width),
      static_cast<uint32_t>(height), 4u, eastl::move(pixels));

  m_texture_cache[key] = asset;

  LOG_INFO("[AssetManager] loaded Texture2D {} (resolved: {}, {}x{}, src_channels={})",
           key.c_str(), absolute.generic_string(), width, height, channels);
  return asset;
}

eastl::shared_ptr<ShaderAsset> AssetManager::loadShader(
    const eastl::string& virtual_path) {
  if (!m_is_initialized) {
    LOG_ERROR("[AssetManager] loadShader before initialize()");
    return nullptr;
  }
  if (virtual_path.empty()) {
    LOG_ERROR("[AssetManager] loadShader: empty path");
    return nullptr;
  }

  const eastl::string key = canonicalKey(virtual_path);
  if (auto it = m_shader_cache.find(key); it != m_shader_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_shader_cache.erase(it);
  }

  const auto absolute =
      m_file_system->resolveShader(std::filesystem::path(key.c_str()));
  eastl::vector<uint8_t> bytes;
  if (!m_file_system->readBinary(absolute, bytes)) {
    LOG_ERROR("[AssetManager] loadShader: cannot read {}",
              absolute.generic_string());
    return nullptr;
  }

  ShaderAsset::Stage stage = ShaderAsset::Stage::Unknown;
  const eastl::string ext = eastl::string(absolute.extension().generic_string().c_str());
  if (ext == ".vert") {
    stage = ShaderAsset::Stage::Vertex;
  } else if (ext == ".frag") {
    stage = ShaderAsset::Stage::Fragment;
  } else if (ext == ".comp") {
    stage = ShaderAsset::Stage::Compute;
  }

  Asset::Meta meta;
  meta.virtual_path = key;
  meta.absolute_path = absolute;
  meta.source_timestamp = querySourceTimestamp(absolute);
  auto asset =
      eastl::make_shared<ShaderAsset>(eastl::move(meta), stage, eastl::move(bytes));
  m_shader_cache[key] = asset;
  LOG_INFO("[AssetManager] loaded Shader {} (resolved: {}, bytes={})",
           key.c_str(), absolute.generic_string(), asset->getByteSize());
  return asset;
}

eastl::shared_ptr<SceneAsset> AssetManager::loadScene(
    const eastl::string& virtual_path) {
  if (!m_is_initialized) {
    LOG_ERROR("[AssetManager] loadScene before initialize()");
    return nullptr;
  }
  if (virtual_path.empty()) {
    LOG_ERROR("[AssetManager] loadScene: empty path");
    return nullptr;
  }

  const eastl::string request_key = canonicalKey(virtual_path);
  const ResolvedContentPath resolved =
      resolveContentPath(m_file_system, virtual_path, false);
  const eastl::string& key = resolved.canonical_key;

  if (auto it = m_scene_cache.find(key); it != m_scene_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_scene_cache.erase(it);
  }

  const auto& absolute = resolved.absolute;
  eastl::string json_text;
  if (!m_file_system->readText(absolute, json_text)) {
    LOG_ERROR("[AssetManager] loadScene: cannot read {}",
              absolute.generic_string());
    return nullptr;
  }

  Scene scene;
  if (!SceneSerializer::deserialize(json_text, scene)) {
    LOG_ERROR("[AssetManager] loadScene: deserialize failed for {}",
              absolute.generic_string());
    return nullptr;
  }

  scene.setName(eastl::string(absolute.stem().generic_string().c_str()));

  Asset::Meta meta;
  meta.virtual_path = key;
  meta.absolute_path = absolute;
  meta.source_timestamp = querySourceTimestamp(absolute);
  auto asset = eastl::make_shared<SceneAsset>(eastl::move(meta), eastl::move(scene));
  m_scene_cache[key] = asset;

  LOG_INFO("[AssetManager] loaded Scene {} (resolved: {}, entities={}, childScenes={})",
           key.c_str(), absolute.generic_string(), asset->getScene().getEntities().size(),
           asset->getScene().getChildScenes().size());
  return asset;
}

bool AssetManager::openGltfImportDocument(const eastl::string& virtual_path,
                                          GltfImportDocument& out_document) {
  out_document = {};
  if (!m_is_initialized || virtual_path.empty()) {
    return false;
  }

  const ResolvedContentPath resolved =
      resolveContentPath(m_file_system, virtual_path, true);
  const eastl::string mesh_extension(
      resolved.absolute.extension().generic_string().c_str());
  if (mesh_extension != ".gltf" && mesh_extension != ".glb") {
    LOG_ERROR("[AssetManager] openGltfImportDocument: unsupported format {}",
              resolved.absolute.generic_string());
    return false;
  }

  eastl::vector<uint8_t> bytes;
  if (!m_file_system->readBinary(resolved.absolute, bytes)) {
    LOG_ERROR("[AssetManager] openGltfImportDocument: cannot read {}",
              resolved.absolute.generic_string());
    return false;
  }

  cgltf_options options{};
  cgltf_data* data = nullptr;
  const cgltf_result parse_result =
      cgltf_parse(&options, bytes.data(), bytes.size(), &data);
  if (parse_result != cgltf_result_success || data == nullptr) {
    LOG_ERROR("[AssetManager] openGltfImportDocument: cgltf_parse failed for {} ({})",
              resolved.absolute.generic_string(), cgltfResultToString(parse_result));
    return false;
  }

  const std::string absolute_string = resolved.absolute.string();
  const cgltf_result buffer_result =
      cgltf_load_buffers(&options, data, absolute_string.c_str());
  if (buffer_result != cgltf_result_success) {
    cgltf_free(data);
    LOG_ERROR(
        "[AssetManager] openGltfImportDocument: cgltf_load_buffers failed for {} ({})",
        resolved.absolute.generic_string(), cgltfResultToString(buffer_result));
    return false;
  }

  const cgltf_result validate_result = cgltf_validate(data);
  if (validate_result != cgltf_result_success) {
    cgltf_free(data);
    LOG_ERROR(
        "[AssetManager] openGltfImportDocument: cgltf_validate failed for {} ({})",
        resolved.absolute.generic_string(), cgltfResultToString(validate_result));
    return false;
  }

  out_document.absolute = resolved.absolute;
  out_document.canonical_key = resolved.canonical_key;
  out_document.data = data;
  return true;
}

void AssetManager::closeGltfImportDocument(GltfImportDocument& document) {
  if (document.data != nullptr) {
    cgltf_free(document.data);
    document.data = nullptr;
  }
}

eastl::shared_ptr<MeshAsset> AssetManager::loadMesh(
    const eastl::string& virtual_path) {
  if (!m_is_initialized) {
    LOG_ERROR("[AssetManager] loadMesh before initialize()");
    return nullptr;
  }
  if (virtual_path.empty()) {
    LOG_ERROR("[AssetManager] loadMesh: empty path");
    return nullptr;
  }

  const eastl::string request_key = canonicalKey(virtual_path);
  const auto endsWithSuffix = [](const eastl::string& value, const char* suffix) {
    const size_t suffix_length = std::strlen(suffix);
    if (value.size() < suffix_length) {
      return false;
    }
    return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
           0;
  };
  if (endsWithSuffix(request_key, ".mesh.asset")) {
    const ResolvedContentPath descriptor_path =
        resolveContentPath(m_file_system, virtual_path, false);
    eastl::string descriptor_text;
    if (!m_file_system->readText(descriptor_path.absolute, descriptor_text)) {
      LOG_ERROR("[AssetManager] loadMesh: cannot read mesh descriptor {}",
                descriptor_path.absolute.generic_string());
      return nullptr;
    }

    eastl::string source_path;
    if (!parseMeshAssetSource(descriptor_text, source_path)) {
      LOG_ERROR("[AssetManager] loadMesh: missing source in {}",
                descriptor_path.absolute.generic_string());
      return nullptr;
    }
    return loadMesh(source_path);
  }

  const ResolvedContentPath resolved =
      resolveContentPath(m_file_system, virtual_path, true);
  const eastl::string& key = resolved.canonical_key;
  if (auto it = m_mesh_cache.find(key); it != m_mesh_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_mesh_cache.erase(it);
  }

  const auto& absolute = resolved.absolute;
  eastl::vector<uint8_t> bytes;
  if (!m_file_system->readBinary(absolute, bytes)) {
    LOG_ERROR("[AssetManager] loadMesh: cannot read {}", absolute.generic_string());
    return nullptr;
  }

  const eastl::string mesh_extension(
      absolute.extension().generic_string().c_str());
  if (mesh_extension != ".gltf" && mesh_extension != ".glb") {
    LOG_ERROR("[AssetManager] loadMesh: unsupported mesh format {} for {}",
              mesh_extension.c_str(), absolute.generic_string());
    return nullptr;
  }

  cgltf_options options{};
  cgltf_data* data = nullptr;
  const auto fail_mesh_load = [&](const char* format,
                                  auto&&... args) -> eastl::shared_ptr<MeshAsset> {
    LOG_ERROR("{}", fmt::vformat(fmt::string_view(format),
                                  fmt::make_format_args(args...)));
    if (data != nullptr) {
      cgltf_free(data);
      data = nullptr;
    }
    return nullptr;
  };

  const cgltf_result parse_result =
      cgltf_parse(&options, bytes.data(), bytes.size(), &data);
  if (parse_result != cgltf_result_success || data == nullptr) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: cgltf_parse failed for {} ({})",
        absolute.generic_string(), cgltfResultToString(parse_result));
  }

  const std::string absolute_string = absolute.string();
  const cgltf_result buffer_result =
      cgltf_load_buffers(&options, data, absolute_string.c_str());
  if (buffer_result != cgltf_result_success) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: cgltf_load_buffers failed for {} ({})",
        absolute.generic_string(), cgltfResultToString(buffer_result));
  }

  const cgltf_result validate_result = cgltf_validate(data);
  if (validate_result != cgltf_result_success) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: cgltf_validate failed for {} ({})",
        absolute.generic_string(), cgltfResultToString(validate_result));
  }

  size_t selected_mesh_index = 0;
  size_t selected_primitive_index = 0;
  bool found_primitive = false;
  for (cgltf_size mesh_index = 0; mesh_index < data->meshes_count; ++mesh_index) {
    const cgltf_mesh& mesh = data->meshes[mesh_index];
    for (cgltf_size primitive_index = 0;
         primitive_index < mesh.primitives_count; ++primitive_index) {
      if (!found_primitive) {
        selected_mesh_index = static_cast<size_t>(mesh_index);
        selected_primitive_index = static_cast<size_t>(primitive_index);
        found_primitive = true;
        break;
      }
    }
    if (found_primitive) {
      break;
    }
  }

  if (!found_primitive) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: no mesh primitives found in {}",
        absolute.generic_string());
  }

  const eastl::shared_ptr<MeshAsset> loaded = loadMeshPrimitive(
      data, selected_mesh_index, selected_primitive_index, absolute, key);
  cgltf_free(data);
  if (loaded) {
    LOG_INFO("[AssetManager] loaded Mesh {} (resolved: {}, mesh_index={}, primitive_index={})",
             key.c_str(), absolute.generic_string(), selected_mesh_index,
             selected_primitive_index);
  }
  return loaded;
}

AssetHandle AssetManager::makeHandle(Asset::Type type,
                                     const eastl::string& virtual_path) const {
  AssetHandle handle;
  handle.type = type;
  handle.key = canonicalKey(virtual_path);
  handle.generation = 0;
  return handle;
}

eastl::shared_ptr<Asset> AssetManager::get(const AssetHandle& handle) const {
  if (!handle.isValid()) {
    return nullptr;
  }

  switch (handle.type) {
    case Asset::Type::Texture2D: {
      auto it = m_texture_cache.find(handle.key);
      if (it == m_texture_cache.end()) {
        return nullptr;
      }
      auto ptr = it->second.lock();
      if (!ptr) {
        return nullptr;
      }
      return eastl::static_pointer_cast<Asset>(ptr);
    }
    case Asset::Type::Shader: {
      auto it = m_shader_cache.find(handle.key);
      if (it == m_shader_cache.end()) {
        return nullptr;
      }
      auto ptr = it->second.lock();
      if (!ptr) {
        return nullptr;
      }
      return eastl::static_pointer_cast<Asset>(ptr);
    }
    case Asset::Type::Mesh: {
      auto it = m_mesh_cache.find(handle.key);
      if (it == m_mesh_cache.end()) {
        return nullptr;
      }
      auto ptr = it->second.lock();
      if (!ptr) {
        return nullptr;
      }
      return eastl::static_pointer_cast<Asset>(ptr);
    }
    case Asset::Type::Material: {
      auto it = m_material_cache.find(handle.key);
      if (it == m_material_cache.end()) {
        return nullptr;
      }
      auto ptr = it->second.lock();
      if (!ptr) {
        return nullptr;
      }
      return eastl::static_pointer_cast<Asset>(ptr);
    }
    case Asset::Type::Scene: {
      auto it = m_scene_cache.find(handle.key);
      if (it == m_scene_cache.end()) {
        return nullptr;
      }
      auto ptr = it->second.lock();
      if (!ptr) {
        return nullptr;
      }
      return eastl::static_pointer_cast<Asset>(ptr);
    }
    default:
      return nullptr;
  }
}

void AssetManager::garbageCollect() {
  for (auto it = m_texture_cache.begin(); it != m_texture_cache.end();) {
    if (it->second.expired()) {
      it = m_texture_cache.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_shader_cache.begin(); it != m_shader_cache.end();) {
    if (it->second.expired()) {
      it = m_shader_cache.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_mesh_cache.begin(); it != m_mesh_cache.end();) {
    if (it->second.expired()) {
      it = m_mesh_cache.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_material_cache.begin(); it != m_material_cache.end();) {
    if (it->second.expired()) {
      it = m_material_cache.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_scene_cache.begin(); it != m_scene_cache.end();) {
    if (it->second.expired()) {
      it = m_scene_cache.erase(it);
    } else {
      ++it;
    }
  }
}

void AssetManager::clearCache() {
  m_texture_cache.clear();
  m_shader_cache.clear();
  m_mesh_cache.clear();
  m_material_cache.clear();
  m_scene_cache.clear();
}

eastl::string AssetManager::canonicalKey(const eastl::string& virtual_path) const {
  const std::filesystem::path normalized =
      std::filesystem::path(virtual_path.c_str()).lexically_normal();
  return eastl::string(normalized.generic_string().c_str());
}

}  // namespace Blunder
