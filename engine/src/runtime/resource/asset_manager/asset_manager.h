#pragma once

#include <filesystem>

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

class FileSystem;

struct AssetManagerInitInfo {
  /// Required. AssetManager performs all IO through this.
  FileSystem* file_system{nullptr};
};

/// AssetManager owns the lifetime of CPU-side resources loaded from disk.
///
/// Lookup is by virtual path (relative to the FileSystem's asset root). A
/// per-type weak cache lets identical paths share a single in-memory copy
/// while still letting assets be released once nothing holds a strong handle.
class AssetManager final {
 public:
  AssetManager() = default;
  ~AssetManager() = default;

  AssetManager(const AssetManager&) = delete;
  AssetManager& operator=(const AssetManager&) = delete;

  void initialize(const AssetManagerInitInfo& info);
  void shutdown();

  // ---- Loading -----------------------------------------------------------

  /// Load (or fetch from cache) a 2D texture. `virtual_path` is interpreted
  /// relative to the FileSystem's asset root; absolute paths are also
  /// accepted. Returns nullptr on failure (the call site is responsible for
  /// reacting; the manager logs the cause).
  ///
  /// The returned texture is RGBA8 (4 channels), regardless of source
  /// channel count, to keep upload paths uniform.
  eastl::shared_ptr<Texture2DAsset> loadTexture2D(
      const eastl::string& virtual_path);

  // ---- Maintenance -------------------------------------------------------

  /// Drop expired cache entries (entries whose only reference was the cache
  /// itself). Safe to call any time; cheap O(n) over the cache.
  void garbageCollect();

  /// Drop all cached references. Strong handles held elsewhere stay valid
  /// until their owners release them.
  void clearCache();

 private:
  template <typename T>
  using Cache = eastl::unordered_map<eastl::string, eastl::weak_ptr<T>>;

  FileSystem* m_file_system{nullptr};
  Cache<Texture2DAsset> m_texture_cache;
  bool m_is_initialized{false};
};

}  // namespace Blunder
