#pragma once

#include <cstdint>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cgltf.h>

#include "EASTL/shared_ptr.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

/// glTF-backed material for forward rendering (Blinn-Phong fallback + PBR textures).
class MaterialAsset final : public Asset {
 public:
  MaterialAsset(Asset::Meta meta,
                const glm::vec4& base_color_factor = glm::vec4(1.0f),
                AssetHandle base_color_texture = {},
                eastl::shared_ptr<Texture2DAsset> base_color_texture_asset =
                    nullptr,
                eastl::shared_ptr<Texture2DAsset> metallic_roughness_texture_asset =
                    nullptr,
                eastl::shared_ptr<Texture2DAsset> normal_texture_asset = nullptr,
                eastl::shared_ptr<Texture2DAsset> occlusion_texture_asset = nullptr,
                const glm::vec3& ambient_color = glm::vec3(0.15f),
                const glm::vec3& diffuse_color = glm::vec3(1.0f),
                const glm::vec3& specular_color = glm::vec3(0.4f),
                float shininess = 32.0f,
                float metallic_factor = 1.0f,
                float roughness_factor = 1.0f,
                cgltf_alpha_mode alpha_mode = cgltf_alpha_mode_opaque,
                float alpha_cutoff = 0.5f,
                bool double_sided = false,
                bool unlit = false)
      : Asset(Asset::Type::Material, eastl::move(meta)),
        m_base_color_factor(base_color_factor),
        m_base_color_texture(eastl::move(base_color_texture)),
        m_base_color_texture_asset(eastl::move(base_color_texture_asset)),
        m_metallic_roughness_texture_asset(
            eastl::move(metallic_roughness_texture_asset)),
        m_normal_texture_asset(eastl::move(normal_texture_asset)),
        m_occlusion_texture_asset(eastl::move(occlusion_texture_asset)),
        m_ambient_color(ambient_color),
        m_diffuse_color(diffuse_color),
        m_specular_color(specular_color),
        m_shininess(shininess),
        m_metallic_factor(metallic_factor),
        m_roughness_factor(roughness_factor),
        m_alpha_mode(alpha_mode),
        m_alpha_cutoff(alpha_cutoff),
        m_double_sided(double_sided),
        m_unlit(unlit) {
    setState(State::Loaded);
  }

  const glm::vec4& getBaseColorFactor() const { return m_base_color_factor; }
  const AssetHandle& getBaseColorTexture() const { return m_base_color_texture; }
  const eastl::shared_ptr<Texture2DAsset>& getBaseColorTextureAsset() const {
    return m_base_color_texture_asset;
  }
  const eastl::shared_ptr<Texture2DAsset>& getMetallicRoughnessTextureAsset() const {
    return m_metallic_roughness_texture_asset;
  }
  const eastl::shared_ptr<Texture2DAsset>& getNormalTextureAsset() const {
    return m_normal_texture_asset;
  }
  const eastl::shared_ptr<Texture2DAsset>& getOcclusionTextureAsset() const {
    return m_occlusion_texture_asset;
  }
  bool hasBaseColorTexture() const {
    return m_base_color_texture.isValid() || m_base_color_texture_asset != nullptr;
  }
  bool hasMetallicRoughnessTexture() const {
    return m_metallic_roughness_texture_asset != nullptr;
  }
  bool hasNormalTexture() const { return m_normal_texture_asset != nullptr; }
  bool hasOcclusionTexture() const { return m_occlusion_texture_asset != nullptr; }

  const glm::vec3& getAmbientColor() const { return m_ambient_color; }
  const glm::vec3& getDiffuseColor() const { return m_diffuse_color; }
  const glm::vec3& getSpecularColor() const { return m_specular_color; }
  float getShininess() const { return m_shininess; }
  float getMetallicFactor() const { return m_metallic_factor; }
  float getRoughnessFactor() const { return m_roughness_factor; }
  cgltf_alpha_mode getAlphaMode() const { return m_alpha_mode; }
  float getAlphaCutoff() const { return m_alpha_cutoff; }
  bool isDoubleSided() const { return m_double_sided; }
  bool isUnlit() const { return m_unlit; }
  bool isBlendTransparent() const {
    return m_alpha_mode == cgltf_alpha_mode_blend;
  }
  bool isMaskTransparent() const {
    return m_alpha_mode == cgltf_alpha_mode_mask;
  }

 private:
  glm::vec4 m_base_color_factor{1.0f};
  AssetHandle m_base_color_texture;
  eastl::shared_ptr<Texture2DAsset> m_base_color_texture_asset;
  eastl::shared_ptr<Texture2DAsset> m_metallic_roughness_texture_asset;
  eastl::shared_ptr<Texture2DAsset> m_normal_texture_asset;
  eastl::shared_ptr<Texture2DAsset> m_occlusion_texture_asset;
  glm::vec3 m_ambient_color{0.15f};
  glm::vec3 m_diffuse_color{1.0f};
  glm::vec3 m_specular_color{0.4f};
  float m_shininess{32.0f};
  float m_metallic_factor{1.0f};
  float m_roughness_factor{1.0f};
  cgltf_alpha_mode m_alpha_mode{cgltf_alpha_mode_opaque};
  float m_alpha_cutoff{0.5f};
  bool m_double_sided{false};
  bool m_unlit{false};
};

}  // namespace Blunder
