#pragma once

#include <cstdint>

#include "EASTL/vector.h"

#include "runtime/resource/asset/asset.h"

namespace Blunder {

/// CPU-side mesh resource skeleton.
/// Phase1 keeps only generic vertex/index blobs.
class MeshAsset final : public Asset {
 public:
  MeshAsset(Asset::Meta meta, eastl::vector<uint8_t> vertex_blob,
            uint32_t vertex_stride, eastl::vector<uint32_t> indices)
      : Asset(Asset::Type::Mesh, eastl::move(meta)),
        m_vertex_blob(eastl::move(vertex_blob)),
        m_vertex_stride(vertex_stride),
        m_indices(eastl::move(indices)) {
    setState(State::Loaded);
  }

  const eastl::vector<uint8_t>& getVertexBlob() const { return m_vertex_blob; }
  uint32_t getVertexStride() const { return m_vertex_stride; }
  const eastl::vector<uint32_t>& getIndices() const { return m_indices; }
  size_t getVertexByteSize() const { return m_vertex_blob.size(); }
  size_t getIndexCount() const { return m_indices.size(); }

 private:
  eastl::vector<uint8_t> m_vertex_blob;
  uint32_t m_vertex_stride{0};
  eastl::vector<uint32_t> m_indices;
};

}  // namespace Blunder
