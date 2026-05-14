#pragma once

#include <cstdint>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "EASTL/shared_ptr.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset/material_asset.h"

namespace Blunder {

struct MeshVertex {
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 0.0f, 1.0f};
  glm::vec2 uv{0.0f};
};

/// CPU-side mesh resource used by the importer before any render-backend
/// specific vertex conversion happens.
class MeshAsset final : public Asset {
 public:
  MeshAsset(Asset::Meta meta, eastl::vector<MeshVertex> vertices,
            eastl::vector<uint32_t> indices, AssetHandle material = {},
            eastl::shared_ptr<MaterialAsset> material_asset = nullptr)
      : Asset(Asset::Type::Mesh, eastl::move(meta)),
        m_vertices(eastl::move(vertices)),
        m_indices(eastl::move(indices)),
        m_material(eastl::move(material)),
        m_material_asset(eastl::move(material_asset)) {
    setState(State::Loaded);
  }

  const eastl::vector<MeshVertex>& getVertices() const { return m_vertices; }
  const MeshVertex* getVertexData() const { return m_vertices.data(); }
  uint32_t getVertexStride() const { return sizeof(MeshVertex); }
  const eastl::vector<uint32_t>& getIndices() const { return m_indices; }
  const AssetHandle& getMaterial() const { return m_material; }
  const eastl::shared_ptr<MaterialAsset>& getMaterialAsset() const {
    return m_material_asset;
  }
  bool hasMaterial() const {
    return m_material.isValid() || m_material_asset != nullptr;
  }
  size_t getVertexCount() const { return m_vertices.size(); }
  size_t getVertexByteSize() const { return m_vertices.size() * sizeof(MeshVertex); }
  size_t getIndexCount() const { return m_indices.size(); }

 private:
  eastl::vector<MeshVertex> m_vertices;
  eastl::vector<uint32_t> m_indices;
  AssetHandle m_material;
  eastl::shared_ptr<MaterialAsset> m_material_asset;
};

}  // namespace Blunder
