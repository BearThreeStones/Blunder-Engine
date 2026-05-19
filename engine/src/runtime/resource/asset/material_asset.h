#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "EASTL/shared_ptr.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

/// glTF-backed material with Blinn-Phong parameters for the mesh pipeline.
class MaterialAsset final : public Asset {
 public:
  MaterialAsset(Asset::Meta meta,
                const glm::vec4& base_color_factor = glm::vec4(1.0f),
                AssetHandle base_color_texture = {},
                eastl::shared_ptr<Texture2DAsset> base_color_texture_asset =
                    nullptr,
                const glm::vec3& ambient_color = glm::vec3(0.15f),
                const glm::vec3& diffuse_color = glm::vec3(1.0f),
                const glm::vec3& specular_color = glm::vec3(0.4f),
                float shininess = 32.0f,
                bool unlit = false)
      : Asset(Asset::Type::Material, eastl::move(meta)),
        m_base_color_factor(base_color_factor),
        m_base_color_texture(eastl::move(base_color_texture)),
        m_base_color_texture_asset(eastl::move(base_color_texture_asset)),
        m_ambient_color(ambient_color),
        m_diffuse_color(diffuse_color),
        m_specular_color(specular_color),
        m_shininess(shininess),
        m_unlit(unlit) {
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

  const glm::vec3& getAmbientColor() const { return m_ambient_color; }
  const glm::vec3& getDiffuseColor() const { return m_diffuse_color; }
  const glm::vec3& getSpecularColor() const { return m_specular_color; }
  float getShininess() const { return m_shininess; }
  bool isUnlit() const { return m_unlit; }

 private:
  glm::vec4 m_base_color_factor{1.0f};
  AssetHandle m_base_color_texture;
  eastl::shared_ptr<Texture2DAsset> m_base_color_texture_asset;
  glm::vec3 m_ambient_color{0.15f};
  glm::vec3 m_diffuse_color{1.0f};
  glm::vec3 m_specular_color{0.4f};
  float m_shininess{32.0f};
  bool m_unlit{false};
};

}  // namespace Blunder
