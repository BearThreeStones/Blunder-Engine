#pragma once

#include "EASTL/string.h"

namespace Blunder {

/// Base class for every resource the AssetManager hands out.
///
/// Concrete asset types (textures, meshes, materials, ...) inherit from this
/// and add their own typed payload + load/unload code paths. The asset itself
/// owns CPU-side data; uploading to GPU is the responsibility of the consuming
/// system (e.g. RenderSystem builds a VkImage from a Texture2DAsset).
class Asset {
 public:
  enum class Type : uint32_t {
    Unknown = 0,
    Texture2D,
    Mesh,
    Material,
    Shader,
  };

  Asset(Type type, eastl::string name)
      : m_type(type), m_name(eastl::move(name)) {}
  virtual ~Asset() = default;

  Asset(const Asset&) = delete;
  Asset& operator=(const Asset&) = delete;

  Type getType() const { return m_type; }
  const eastl::string& getName() const { return m_name; }

 private:
  Type m_type;
  eastl::string m_name;  // virtual path used as cache key
};

}  // namespace Blunder
