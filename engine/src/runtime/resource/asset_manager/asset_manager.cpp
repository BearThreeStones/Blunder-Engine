#include "runtime/resource/asset_manager/asset_manager.h"

#include <cgltf.h>
#include <spdlog/fmt/fmt.h>
#include <stb_image.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>

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

eastl::string buildAssetVirtualPath(const std::filesystem::path& asset_root,
                                    const std::filesystem::path& source_path,
                                    const char* uri) {
  const std::filesystem::path candidate = std::filesystem::path(uri).is_absolute()
                                              ? std::filesystem::path(uri)
                                              : (source_path.parent_path() / uri);
  const std::filesystem::path normalized = candidate.lexically_normal();

  std::error_code ec;
  const std::filesystem::path relative =
      std::filesystem::relative(normalized, asset_root, ec);
  if (!ec && !relative.empty() && !relativePathEscapesRoot(relative)) {
    return eastl::string(relative.generic_string().c_str());
  }

  return eastl::string(normalized.generic_string().c_str());
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
  LOG_INFO("[AssetManager] initialized (asset root: {})",
           m_file_system->getAssetRoot().generic_string());
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

  const eastl::string key = canonicalKey(virtual_path);
  if (auto it = m_texture_cache.find(key);
      it != m_texture_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_texture_cache.erase(it);
  }

  const auto absolute =
      m_file_system->resolveAsset(std::filesystem::path(key.c_str()));

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

  const eastl::string key = canonicalKey(virtual_path);
  if (auto it = m_mesh_cache.find(key); it != m_mesh_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_mesh_cache.erase(it);
  }

  const auto absolute =
      m_file_system->resolveAsset(std::filesystem::path(key.c_str()));
  eastl::vector<uint8_t> bytes;
  if (!m_file_system->readBinary(absolute, bytes)) {
    LOG_ERROR("[AssetManager] loadMesh: cannot read {}", absolute.generic_string());
    return nullptr;
  }

  const eastl::string extension(
      absolute.extension().generic_string().c_str());
  if (extension != ".gltf" && extension != ".glb") {
    LOG_ERROR("[AssetManager] loadMesh: unsupported mesh format {} for {}",
              extension.c_str(), absolute.generic_string());
    return nullptr;
  }

  cgltf_options options{};
  cgltf_data* data = nullptr;
  const auto fail_mesh_load = [&](const char* format,
                                  auto&&... args) -> eastl::shared_ptr<MeshAsset> {
    LOG_ERROR("{}", fmt::vformat(fmt::runtime(format),
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

  const cgltf_mesh* selected_mesh = nullptr;
  const cgltf_primitive* selected_primitive = nullptr;
  size_t selected_mesh_index = 0;
  size_t selected_primitive_index = 0;
  size_t total_primitives = 0;
  for (cgltf_size mesh_index = 0; mesh_index < data->meshes_count;
       ++mesh_index) {
    const cgltf_mesh& mesh = data->meshes[mesh_index];
    for (cgltf_size primitive_index = 0;
         primitive_index < mesh.primitives_count; ++primitive_index) {
      ++total_primitives;
      if (selected_primitive == nullptr) {
        selected_mesh = &mesh;
        selected_primitive = &mesh.primitives[primitive_index];
        selected_mesh_index = static_cast<size_t>(mesh_index);
        selected_primitive_index = static_cast<size_t>(primitive_index);
      }
    }
  }

  if (selected_primitive == nullptr || selected_mesh == nullptr) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: no mesh primitives found in {}",
        absolute.generic_string());
  }
  if (total_primitives != 1u) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: {} contains {} primitives; only single-primitive meshes are supported for now",
        absolute.generic_string(), total_primitives);
  }

  const cgltf_primitive& primitive = *selected_primitive;
  if (primitive.type != cgltf_primitive_type_triangles) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: {} primitive {} uses unsupported mode {}",
        absolute.generic_string(), selected_primitive_index,
        static_cast<int>(primitive.type));
  }
  if (primitive.has_draco_mesh_compression) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: {} primitive {} uses unsupported Draco compression",
        absolute.generic_string(), selected_primitive_index);
  }

  const cgltf_attribute* position_attribute =
      findPrimitiveAttribute(primitive, cgltf_attribute_type_position, 0);
  const cgltf_attribute* normal_attribute =
      findPrimitiveAttribute(primitive, cgltf_attribute_type_normal, 0);
  const cgltf_attribute* uv_attribute =
      findPrimitiveAttribute(primitive, cgltf_attribute_type_texcoord, 0);
  if (position_attribute == nullptr || position_attribute->data == nullptr) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: {} primitive {} is missing POSITION data",
        absolute.generic_string(), selected_primitive_index);
  }

  const cgltf_accessor* position_accessor = position_attribute->data;
  const cgltf_accessor* normal_accessor =
      normal_attribute != nullptr ? normal_attribute->data : nullptr;
  const cgltf_accessor* uv_accessor =
      uv_attribute != nullptr ? uv_attribute->data : nullptr;
  const size_t vertex_count = static_cast<size_t>(position_accessor->count);
  if (vertex_count == 0u) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: {} primitive {} has zero vertices",
        absolute.generic_string(), selected_primitive_index);
  }
  if (normal_accessor != nullptr &&
      static_cast<size_t>(normal_accessor->count) != vertex_count) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: {} primitive {} has mismatched NORMAL count",
        absolute.generic_string(), selected_primitive_index);
  }
  if (uv_accessor != nullptr &&
      static_cast<size_t>(uv_accessor->count) != vertex_count) {
    return fail_mesh_load(
        "[AssetManager] loadMesh: {} primitive {} has mismatched TEXCOORD_0 count",
        absolute.generic_string(), selected_primitive_index);
  }

  eastl::vector<MeshVertex> vertices(vertex_count);
  for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    if (!cgltf_accessor_read_float(position_accessor, vertex_index, position,
                                   3)) {
      return fail_mesh_load(
          "[AssetManager] loadMesh: {} failed reading POSITION at vertex {}",
          absolute.generic_string(), vertex_index);
    }
    vertices[vertex_index].position =
        glm::vec3(position[0], position[1], position[2]);

    if (normal_accessor != nullptr) {
      float normal[3] = {0.0f, 0.0f, 1.0f};
      if (!cgltf_accessor_read_float(normal_accessor, vertex_index, normal,
                                     3)) {
        return fail_mesh_load(
            "[AssetManager] loadMesh: {} failed reading NORMAL at vertex {}",
            absolute.generic_string(), vertex_index);
      }
      vertices[vertex_index].normal = glm::vec3(normal[0], normal[1], normal[2]);
    }

    if (uv_accessor != nullptr) {
      float uv[2] = {0.0f, 0.0f};
      if (!cgltf_accessor_read_float(uv_accessor, vertex_index, uv, 2)) {
        return fail_mesh_load(
            "[AssetManager] loadMesh: {} failed reading TEXCOORD_0 at vertex {}",
            absolute.generic_string(), vertex_index);
      }
      vertices[vertex_index].uv = glm::vec2(uv[0], uv[1]);
    }
  }

  eastl::vector<uint32_t> indices;
  if (primitive.indices != nullptr) {
    const size_t index_count = static_cast<size_t>(primitive.indices->count);
    indices.resize(index_count);
    for (size_t index = 0; index < index_count; ++index) {
      const cgltf_size value = cgltf_accessor_read_index(primitive.indices, index);
      if (value > std::numeric_limits<uint32_t>::max()) {
        return fail_mesh_load(
            "[AssetManager] loadMesh: {} index {} exceeds uint32 range",
            absolute.generic_string(), index);
      }
      indices[index] = static_cast<uint32_t>(value);
    }
  } else {
    indices.resize(vertex_count);
    for (size_t index = 0; index < vertex_count; ++index) {
      indices[index] = static_cast<uint32_t>(index);
    }
  }

  AssetHandle material_handle;
  eastl::shared_ptr<MaterialAsset> material_asset;
  if (primitive.material != nullptr) {
    const size_t material_index =
        static_cast<size_t>(primitive.material - data->materials);
    const eastl::string material_key =
        canonicalKey(buildGeneratedKey(key, "material", material_index));
    if (auto it = m_material_cache.find(material_key);
        it != m_material_cache.end()) {
      material_asset = it->second.lock();
      if (!material_asset) {
        m_material_cache.erase(it);
      }
    }

    if (!material_asset) {
      glm::vec4 base_color_factor(1.0f);
      AssetHandle base_color_texture_handle;
      eastl::shared_ptr<Texture2DAsset> base_color_texture_asset;
      const cgltf_material& material = *primitive.material;

      if (material.has_pbr_metallic_roughness) {
        const cgltf_pbr_metallic_roughness& pbr =
            material.pbr_metallic_roughness;
        base_color_factor =
            glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                      pbr.base_color_factor[2], pbr.base_color_factor[3]);

        const cgltf_texture* base_color_texture =
            pbr.base_color_texture.texture;
        if (base_color_texture != nullptr) {
          const cgltf_image* image = base_color_texture->image;
          if (image == nullptr) {
            return fail_mesh_load(
                "[AssetManager] loadMesh: {} material {} uses unsupported non-URI baseColor image source",
                absolute.generic_string(), material_index);
          }
          if (image->buffer_view != nullptr || image->uri == nullptr ||
              isDataUri(image->uri)) {
            return fail_mesh_load(
                "[AssetManager] loadMesh: {} material {} only supports external image URIs for baseColor textures",
                absolute.generic_string(), material_index);
          }

          const eastl::string texture_virtual_path = buildAssetVirtualPath(
              m_file_system->getAssetRoot(), absolute, image->uri);
          base_color_texture_asset = loadTexture2D(texture_virtual_path);
          if (!base_color_texture_asset) {
            return fail_mesh_load(
                "[AssetManager] loadMesh: {} failed loading baseColor texture {}",
                absolute.generic_string(), texture_virtual_path.c_str());
          }
          base_color_texture_handle =
              makeHandle(Asset::Type::Texture2D, texture_virtual_path);
        }
      }

      Asset::Meta material_meta;
      material_meta.virtual_path = material_key;
      material_meta.absolute_path = absolute;
      material_meta.source_timestamp = querySourceTimestamp(absolute);
      material_asset = eastl::make_shared<MaterialAsset>(
          eastl::move(material_meta), base_color_factor,
          base_color_texture_handle, base_color_texture_asset);
      m_material_cache[material_key] = material_asset;
    }

    material_handle = makeHandle(Asset::Type::Material, material_key);
  }

  Asset::Meta meta;
  meta.virtual_path = key;
  meta.absolute_path = absolute;
  meta.source_timestamp = querySourceTimestamp(absolute);
  auto asset = eastl::make_shared<MeshAsset>(
      eastl::move(meta), eastl::move(vertices), eastl::move(indices),
      material_handle, material_asset);
  m_mesh_cache[key] = asset;

  LOG_INFO(
      "[AssetManager] loaded Mesh {} (resolved: {}, mesh={}, mesh_index={}, primitive_index={}, vertices={}, indices={}, material={}, baseColorTexture={})",
      key.c_str(), absolute.generic_string(),
      selected_mesh->name != nullptr ? selected_mesh->name : "<unnamed>",
      selected_mesh_index, selected_primitive_index, asset->getVertexCount(),
      asset->getIndexCount(), asset->hasMaterial() ? "yes" : "no",
      material_asset && material_asset->hasBaseColorTexture() ? "yes" : "no");

  cgltf_free(data);
  return asset;
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
}

void AssetManager::clearCache() {
  m_texture_cache.clear();
  m_shader_cache.clear();
  m_mesh_cache.clear();
  m_material_cache.clear();
}

eastl::string AssetManager::canonicalKey(const eastl::string& virtual_path) const {
  const std::filesystem::path normalized =
      std::filesystem::path(virtual_path.c_str()).lexically_normal();
  return eastl::string(normalized.generic_string().c_str());
}

}  // namespace Blunder
