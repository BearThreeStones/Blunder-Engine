#include "runtime/resource/asset_manager/asset_manager.h"

#include "runtime/resource/asset_manager/asset_manager_gltf.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <glm/glm.hpp>

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

bool relativePathEscapesRoot(const std::filesystem::path& relative_path) {
  for (const auto& part : relative_path.lexically_normal()) {
    if (part == ".") {
      continue;
    }
    return part == "..";
  }
  return false;
}

eastl::string buildResourceVirtualPath(const std::filesystem::path& resources_root,
                                         const std::filesystem::path& source_path,
                                         const char* uri) {
  const std::filesystem::path candidate =
      std::filesystem::path(uri).is_absolute()
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

bool isDataUri(const char* uri) {
  return uri != nullptr && std::strncmp(uri, "data:", 5) == 0;
}

void computeMeshTangents(eastl::vector<MeshVertex>& vertices,
                         const eastl::vector<uint32_t>& indices) {
  eastl::vector<glm::vec3> tan1(vertices.size(), glm::vec3(0.0f));
  eastl::vector<glm::vec3> tan2(vertices.size(), glm::vec3(0.0f));

  for (size_t index = 0; index + 2 < indices.size(); index += 3) {
    const uint32_t i0 = indices[index];
    const uint32_t i1 = indices[index + 1];
    const uint32_t i2 = indices[index + 2];
    if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
      continue;
    }

    const glm::vec3& p0 = vertices[i0].position;
    const glm::vec3& p1 = vertices[i1].position;
    const glm::vec3& p2 = vertices[i2].position;
    const glm::vec2& uv0 = vertices[i0].uv;
    const glm::vec2& uv1 = vertices[i1].uv;
    const glm::vec2& uv2 = vertices[i2].uv;

    const glm::vec3 edge1 = p1 - p0;
    const glm::vec3 edge2 = p2 - p0;
    const glm::vec2 delta_uv1 = uv1 - uv0;
    const glm::vec2 delta_uv2 = uv2 - uv0;
    const float denom =
        delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
    if (std::abs(denom) < 1e-8f) {
      continue;
    }
    const float r = 1.0f / denom;
    const glm::vec3 tangent =
        (edge1 * delta_uv2.y - edge2 * delta_uv1.y) * r;
    const glm::vec3 bitangent =
        (edge2 * delta_uv1.x - edge1 * delta_uv2.x) * r;

    tan1[i0] += tangent;
    tan1[i1] += tangent;
    tan1[i2] += tangent;
    tan2[i0] += bitangent;
    tan2[i1] += bitangent;
    tan2[i2] += bitangent;
  }

  for (size_t vertex_index = 0; vertex_index < vertices.size(); ++vertex_index) {
    const glm::vec3& normal = vertices[vertex_index].normal;
    const glm::vec3 tangent = glm::normalize(tan1[vertex_index]);
    const glm::vec3 bitangent = glm::normalize(tan2[vertex_index]);
    const float handedness =
        (glm::dot(glm::cross(normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;
    vertices[vertex_index].tangent =
        glm::vec4(tangent, handedness);
  }
}

eastl::string buildGeneratedKey(const eastl::string& base_key, const char* kind,
                                size_t index) {
  char suffix[64];
  std::snprintf(suffix, sizeof(suffix), ".generated.%s.%zu", kind, index);
  eastl::string generated_key(base_key);
  generated_key.append(suffix);
  return generated_key;
}

}  // namespace

eastl::string makeMeshPrimitiveCacheKey(const eastl::string& gltf_canonical_key,
                                        size_t mesh_index, size_t primitive_index) {
  char suffix[64];
  std::snprintf(suffix, sizeof(suffix), "#mesh%zu#prim%zu", mesh_index,
                primitive_index);
  eastl::string key(gltf_canonical_key);
  key.append(suffix);
  return key;
}

eastl::string makeGltfMaterialCacheKey(const eastl::string& gltf_canonical_key,
                                       size_t material_index) {
  return buildGeneratedKey(gltf_canonical_key, "material", material_index);
}

eastl::shared_ptr<MaterialAsset> AssetManager::loadGltfMaterial(
    cgltf_data* data, size_t material_index, const std::filesystem::path& absolute,
    const eastl::string& gltf_canonical_key) {
  if (!m_is_initialized || data == nullptr ||
      material_index >= static_cast<size_t>(data->materials_count)) {
    return nullptr;
  }

  const eastl::string material_key =
      makeGltfMaterialCacheKey(gltf_canonical_key, material_index);
  if (auto it = m_material_cache.find(material_key); it != m_material_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_material_cache.erase(it);
  }

  const cgltf_material& material = data->materials[material_index];

  glm::vec4 base_color_factor(1.0f);
  AssetHandle base_color_texture_handle;
  eastl::shared_ptr<Texture2DAsset> base_color_texture_asset;
  eastl::shared_ptr<Texture2DAsset> metallic_roughness_texture_asset;
  eastl::shared_ptr<Texture2DAsset> normal_texture_asset;
  eastl::shared_ptr<Texture2DAsset> occlusion_texture_asset;
  glm::vec3 ambient_color(0.15f);
  glm::vec3 diffuse_color(1.0f);
  glm::vec3 specular_color(0.4f);
  float shininess = 32.0f;
  float metallic_factor = 1.0f;
  float roughness_factor = 1.0f;
  bool unlit = material.unlit != 0;
  cgltf_alpha_mode alpha_mode = material.alpha_mode;
  float alpha_cutoff = material.alpha_cutoff;
  bool double_sided = material.double_sided != 0;

  const auto loadGltfImageTexture =
      [&](const cgltf_texture* texture) -> eastl::shared_ptr<Texture2DAsset> {
        if (texture == nullptr || texture->image == nullptr) {
          return nullptr;
        }
        const cgltf_image* image = texture->image;
        if (image->buffer_view != nullptr || image->uri == nullptr ||
            isDataUri(image->uri)) {
          LOG_WARN(
              "[AssetManager] loadGltfMaterial: {} material {} skipped embedded/data URI texture",
              absolute.generic_string(), material_index);
          return nullptr;
        }
        const eastl::string texture_virtual_path = buildResourceVirtualPath(
            m_file_system->getResourcesRoot(), absolute, image->uri);
        return loadTexture2D(texture_virtual_path);
      };

  if (material.has_pbr_specular_glossiness) {
    const cgltf_pbr_specular_glossiness& sg = material.pbr_specular_glossiness;
    base_color_factor =
        glm::vec4(sg.diffuse_factor[0], sg.diffuse_factor[1], sg.diffuse_factor[2],
                  sg.diffuse_factor[3]);
    specular_color = glm::vec3(sg.specular_factor[0], sg.specular_factor[1],
                               sg.specular_factor[2]);
    shininess = std::clamp(sg.glossiness_factor * 128.0f, 8.0f, 256.0f);
    base_color_texture_asset = loadGltfImageTexture(sg.diffuse_texture.texture);
  } else if (material.has_pbr_metallic_roughness) {
    const cgltf_pbr_metallic_roughness& pbr = material.pbr_metallic_roughness;
    base_color_factor =
        glm::vec4(pbr.base_color_factor[0], pbr.base_color_factor[1],
                  pbr.base_color_factor[2], pbr.base_color_factor[3]);
    metallic_factor = pbr.metallic_factor;
    roughness_factor = pbr.roughness_factor;
    const float metallic = metallic_factor;
    specular_color =
        glm::vec3(0.04f) * (1.0f - metallic) +
        glm::vec3(base_color_factor.x, base_color_factor.y, base_color_factor.z) *
            metallic;
    shininess = 8.0f + (256.0f - 8.0f) * (1.0f - roughness_factor);
    base_color_texture_asset =
        loadGltfImageTexture(pbr.base_color_texture.texture);
    metallic_roughness_texture_asset =
        loadGltfImageTexture(pbr.metallic_roughness_texture.texture);
  }

  normal_texture_asset = loadGltfImageTexture(material.normal_texture.texture);
  occlusion_texture_asset = loadGltfImageTexture(material.occlusion_texture.texture);

  if (base_color_texture_asset) {
    base_color_texture_handle =
        makeHandle(Asset::Type::Texture2D, base_color_texture_asset->getVirtualPath());
  }

  Asset::Meta material_meta;
  material_meta.virtual_path = material_key;
  material_meta.absolute_path = absolute;
  material_meta.source_timestamp = querySourceTimestamp(absolute);
  auto material_asset = eastl::make_shared<MaterialAsset>(
      eastl::move(material_meta), base_color_factor, base_color_texture_handle,
      base_color_texture_asset, metallic_roughness_texture_asset,
      normal_texture_asset, occlusion_texture_asset, ambient_color, diffuse_color,
      specular_color, shininess, metallic_factor, roughness_factor, alpha_mode,
      alpha_cutoff, double_sided, unlit);
  m_material_cache[material_key] = material_asset;
  return material_asset;
}

eastl::shared_ptr<MeshAsset> AssetManager::loadMeshPrimitive(
    cgltf_data* data, size_t mesh_index, size_t primitive_index,
    const std::filesystem::path& absolute, const eastl::string& gltf_canonical_key) {
  if (!m_is_initialized || data == nullptr ||
      mesh_index >= static_cast<size_t>(data->meshes_count)) {
    return nullptr;
  }

  const cgltf_mesh& mesh = data->meshes[mesh_index];
  if (primitive_index >= static_cast<size_t>(mesh.primitives_count)) {
    return nullptr;
  }

  const eastl::string cache_key =
      makeMeshPrimitiveCacheKey(gltf_canonical_key, mesh_index, primitive_index);
  if (auto it = m_mesh_cache.find(cache_key); it != m_mesh_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_mesh_cache.erase(it);
  }

  const cgltf_primitive& primitive = mesh.primitives[primitive_index];
  if (primitive.type != cgltf_primitive_type_triangles) {
    LOG_WARN("[AssetManager] loadMeshPrimitive: {} mesh {} prim {} unsupported topology",
             absolute.generic_string(), mesh_index, primitive_index);
    return nullptr;
  }
  if (primitive.has_draco_mesh_compression) {
    LOG_WARN("[AssetManager] loadMeshPrimitive: {} mesh {} prim {} has Draco compression",
             absolute.generic_string(), mesh_index, primitive_index);
    return nullptr;
  }

  const cgltf_attribute* position_attribute =
      findPrimitiveAttribute(primitive, cgltf_attribute_type_position, 0);
  const cgltf_attribute* normal_attribute =
      findPrimitiveAttribute(primitive, cgltf_attribute_type_normal, 0);
  const cgltf_attribute* uv_attribute =
      findPrimitiveAttribute(primitive, cgltf_attribute_type_texcoord, 0);
  const cgltf_attribute* tangent_attribute =
      findPrimitiveAttribute(primitive, cgltf_attribute_type_tangent, 0);
  if (position_attribute == nullptr || position_attribute->data == nullptr) {
    return nullptr;
  }

  const cgltf_accessor* position_accessor = position_attribute->data;
  const cgltf_accessor* normal_accessor =
      normal_attribute != nullptr ? normal_attribute->data : nullptr;
  const cgltf_accessor* uv_accessor =
      uv_attribute != nullptr ? uv_attribute->data : nullptr;
  const cgltf_accessor* tangent_accessor =
      tangent_attribute != nullptr ? tangent_attribute->data : nullptr;
  const size_t vertex_count = static_cast<size_t>(position_accessor->count);
  if (vertex_count == 0u) {
    return nullptr;
  }

  eastl::vector<MeshVertex> vertices(vertex_count);
  for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    if (!cgltf_accessor_read_float(position_accessor, vertex_index, position, 3)) {
      return nullptr;
    }
    vertices[vertex_index].position =
        glm::vec3(position[0], position[1], position[2]);

    if (normal_accessor != nullptr) {
      float normal[3] = {0.0f, 0.0f, 1.0f};
      if (cgltf_accessor_read_float(normal_accessor, vertex_index, normal, 3)) {
        vertices[vertex_index].normal = glm::vec3(normal[0], normal[1], normal[2]);
      }
    }

    if (uv_accessor != nullptr) {
      float uv[2] = {0.0f, 0.0f};
      if (cgltf_accessor_read_float(uv_accessor, vertex_index, uv, 2)) {
        vertices[vertex_index].uv = glm::vec2(uv[0], uv[1]);
      }
    }

    if (tangent_accessor != nullptr) {
      float tangent[4] = {1.0f, 0.0f, 0.0f, 1.0f};
      if (cgltf_accessor_read_float(tangent_accessor, vertex_index, tangent, 4)) {
        vertices[vertex_index].tangent =
            glm::vec4(tangent[0], tangent[1], tangent[2], tangent[3]);
      }
    }
  }

  if (tangent_accessor == nullptr) {
    eastl::vector<uint32_t> provisional_indices;
    if (primitive.indices != nullptr) {
      const size_t index_count = static_cast<size_t>(primitive.indices->count);
      provisional_indices.resize(index_count);
      for (size_t index = 0; index < index_count; ++index) {
        provisional_indices[index] =
            static_cast<uint32_t>(cgltf_accessor_read_index(primitive.indices, index));
      }
    } else {
      provisional_indices.resize(vertex_count);
      for (size_t index = 0; index < vertex_count; ++index) {
        provisional_indices[index] = static_cast<uint32_t>(index);
      }
    }
    computeMeshTangents(vertices, provisional_indices);
  }

  eastl::vector<uint32_t> indices;
  if (primitive.indices != nullptr) {
    const size_t index_count = static_cast<size_t>(primitive.indices->count);
    indices.resize(index_count);
    for (size_t index = 0; index < index_count; ++index) {
      const cgltf_size value = cgltf_accessor_read_index(primitive.indices, index);
      if (value > std::numeric_limits<uint32_t>::max()) {
        return nullptr;
      }
      indices[index] = static_cast<uint32_t>(value);
    }
  } else {
    indices.resize(vertex_count);
    for (size_t index = 0; index < vertex_count; ++index) {
      indices[index] = static_cast<uint32_t>(index);
    }
  }

  eastl::shared_ptr<MaterialAsset> material_asset;
  AssetHandle material_handle;
  if (primitive.material != nullptr) {
    const size_t material_index =
        static_cast<size_t>(primitive.material - data->materials);
    material_asset =
        loadGltfMaterial(data, material_index, absolute, gltf_canonical_key);
    if (material_asset) {
      material_handle = makeHandle(Asset::Type::Material, material_asset->getVirtualPath());
    }
  }

  Asset::Meta meta;
  meta.virtual_path = cache_key;
  meta.absolute_path = absolute;
  meta.source_timestamp = querySourceTimestamp(absolute);
  auto asset = eastl::make_shared<MeshAsset>(
      eastl::move(meta), eastl::move(vertices), eastl::move(indices), material_handle,
      material_asset);
  m_mesh_cache[cache_key] = asset;
  return asset;
}

}  // namespace Blunder
