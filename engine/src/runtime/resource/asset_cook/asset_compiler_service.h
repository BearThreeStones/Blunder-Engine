#pragma once

#include <cstdint>

#include "EASTL/string.h"

namespace Blunder {

class AssetManager;
class AssetRegistry;
class FileSystem;

struct AssetCompilerStats {
  uint32_t meshes_cooked{0};
  uint32_t textures_cooked{0};
  uint32_t skipped{0};
  uint32_t failed{0};
};

class AssetCompilerService final {
 public:
  void initialize(FileSystem* file_system, AssetManager* asset_manager,
                  AssetRegistry* asset_registry);
  void shutdown();

  AssetCompilerStats cookAll(bool force = false);
  AssetCompilerStats cookIfStale();

 private:
  bool cookMeshDescriptor(const eastl::string& descriptor_virtual_path,
                          bool force);
  bool cookTextureDescriptor(const eastl::string& descriptor_virtual_path,
                             bool force);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  AssetRegistry* m_asset_registry{nullptr};
  bool m_is_initialized{false};
};

}  // namespace Blunder
