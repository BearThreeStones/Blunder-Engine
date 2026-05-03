#pragma once

#include <cstdint>
#include <filesystem>

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

  enum class State : uint8_t {
    Unloaded = 0,
    Loaded,
    Failed,
  };

  struct Meta {
    eastl::string virtual_path;
    std::filesystem::path absolute_path;
    uint64_t source_timestamp{0};
  };

  Asset(Type type, Meta meta)
      : m_type(type),
        m_state(State::Unloaded),
        m_virtual_path(eastl::move(meta.virtual_path)),
        m_absolute_path(eastl::move(meta.absolute_path)),
        m_source_timestamp(meta.source_timestamp) {}
  virtual ~Asset() = default;

  Asset(const Asset&) = delete;
  Asset& operator=(const Asset&) = delete;

  Type getType() const { return m_type; }
  State getState() const { return m_state; }
  const eastl::string& getName() const { return m_virtual_path; }
  const eastl::string& getVirtualPath() const { return m_virtual_path; }
  const std::filesystem::path& getAbsolutePath() const { return m_absolute_path; }
  uint64_t getSourceTimestamp() const { return m_source_timestamp; }

 protected:
  void setState(State state) { m_state = state; }
  void setSourceTimestamp(uint64_t timestamp) { m_source_timestamp = timestamp; }

 private:
  Type m_type;
  State m_state;
  eastl::string m_virtual_path;  // cache key
  std::filesystem::path m_absolute_path;
  uint64_t m_source_timestamp{0};  // reserved for hot reload
};

}  // namespace Blunder
