#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/mesh_preview/mesh_preview_studio_lights.h"

namespace Blunder {

class AssetManager;
class FileSystem;
class MeshPreviewOffscreenBackend;

struct SceneThumbnailRenderRequest {
  eastl::string scene_virtual_path;
  uint32_t width{128};
  uint32_t height{128};
};

struct SceneThumbnailRenderResult {
  bool ok{false};
  eastl::string error;
  eastl::vector<uint8_t> rgba;
  uint32_t width{0};
  uint32_t height{0};
};

/// Temporary on-disk Scene Asset still (Play camera, square aspect).
class SceneThumbnailRenderService final {
 public:
  void initialize(AssetManager* asset_manager, FileSystem* file_system,
                  MeshPreviewOffscreenBackend* backend);
  void shutdown();

  SceneThumbnailRenderResult renderSceneAsset(
      const SceneThumbnailRenderRequest& request);

 private:
  AssetManager* m_asset_manager{nullptr};
  FileSystem* m_file_system{nullptr};
  MeshPreviewOffscreenBackend* m_backend{nullptr};
  bool m_is_initialized{false};
};

MeshPreviewCameraFrame meshPreviewFrameFromPlayCamera(
    const Vec3& position, const Vec3& forward, float vertical_fov_radians);

}  // namespace Blunder
