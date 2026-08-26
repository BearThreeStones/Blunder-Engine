#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/mesh_preview/mesh_preview_studio_lights.h"
#include "runtime/function/render/scene_thumbnail/i_scene_still_gpu.h"

namespace Blunder {

class AssetManager;
class FileSystem;
class SceneInstance;

struct SceneThumbnailRenderRequest {
  eastl::string scene_virtual_path;
  uint32_t width{128};
  uint32_t height{128};
};

struct SceneStillRequest {
  /// Live document: render this instance (no disk instantiate).
  SceneInstance* live_instance{nullptr};
  /// On-disk: instantiate this Scene Asset (ignored when live_instance is set).
  eastl::string scene_virtual_path;
  uint32_t width{128};
  uint32_t height{128};
  /// Scene Thumbnail Render requires mesh draws; Capture does not.
  bool require_mesh{true};
};

struct SceneThumbnailRenderResult {
  bool ok{false};
  eastl::string error;
  eastl::string failure_code;
  eastl::vector<uint8_t> rgba;
  uint32_t width{0};
  uint32_t height{0};
  MeshPreviewCameraFrame framing{};
};

/// Temporary on-disk Scene Asset still (Play camera, square aspect for thumbs).
class SceneThumbnailRenderService final {
 public:
  void initialize(AssetManager* asset_manager, FileSystem* file_system,
                  ISceneStillGpuBackend* backend);
  void shutdown();

  SceneThumbnailRenderResult renderSceneAsset(
      const SceneThumbnailRenderRequest& request);

  SceneThumbnailRenderResult renderSceneStill(const SceneStillRequest& request);

 private:
  AssetManager* m_asset_manager{nullptr};
  FileSystem* m_file_system{nullptr};
  ISceneStillGpuBackend* m_backend{nullptr};
  bool m_is_initialized{false};
};

MeshPreviewCameraFrame meshPreviewFrameFromPlayCamera(
    const Vec3& position, const Vec3& forward, float vertical_fov_radians);

}  // namespace Blunder
