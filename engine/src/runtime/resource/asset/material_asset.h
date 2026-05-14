#pragma once

#include <glm/vec4.hpp>

#include "EASTL/shared_ptr.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

/// Minimal glTF material payload for the first textured-mesh milestone.
///
/// Phase 1 only tracks baseColor state so imported meshes can reference a
/// texture asset without pulling the render backend into the resource layer.
class MaterialAsset final : public Asset {
 public:
  MaterialAsset(Asset::Meta meta,
                const glm::vec4& base_color_factor = glm::vec4(1.0f),
                AssetHandle base_color_texture = {},
                eastl::shared_ptr<Texture2DAsset> base_color_texture_asset = nullptr)
      : Asset(Asset::Type::Material, eastl::move(meta)),
        m_base_color_factor(base_color_factor),
        m_base_color_texture(eastl::move(base_color_texture)),
        m_base_color_texture_asset(eastl::move(base_color_texture_asset)) {
    setState(State::Loaded);
  }

  const glm::vec4& getBaseColorFactor() const { return m_base_color_factor; }
  const AssetHandle& getBaseColorTexture() const { return m_base_color_texture; }
  const eastl::shared_ptr<Texture2DAsset>& getBaseColorTextureAsset() const {
    return m_base_color_texture_asset;
  }
  bool hasBaseColorTexture() const {
    return m_base_color_texture.isValid() || m_base_color_texture_asset != nullptr;
  }

 private:
  glm::vec4 m_base_color_factor{1.0f};
  AssetHandle m_base_color_texture;
  eastl::shared_ptr<Texture2DAsset> m_base_color_texture_asset;
};

}  // namespace Blunder