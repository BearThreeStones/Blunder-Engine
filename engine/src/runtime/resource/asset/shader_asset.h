#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset.h"

namespace Blunder {

/// CPU-side shader resource skeleton.
/// Phase1 keeps this as raw source/binary bytes metadata only.
class ShaderAsset final : public Asset {
 public:
  enum class Stage : uint8_t {
    Unknown = 0,
    Vertex,
    Fragment,
    Compute,
  };

  ShaderAsset(Asset::Meta meta, Stage stage, eastl::vector<uint8_t> bytes)
      : Asset(Asset::Type::Shader, eastl::move(meta)),
        m_stage(stage),
        m_bytes(eastl::move(bytes)) {
    setState(State::Loaded);
  }

  Stage getStage() const { return m_stage; }
  const eastl::vector<uint8_t>& getBytes() const { return m_bytes; }
  size_t getByteSize() const { return m_bytes.size(); }

 private:
  Stage m_stage{Stage::Unknown};
  eastl::vector<uint8_t> m_bytes;
};

}  // namespace Blunder
