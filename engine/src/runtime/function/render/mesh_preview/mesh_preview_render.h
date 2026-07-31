#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/mesh_preview/mesh_preview_studio_lights.h"
#include "runtime/resource/asset/mesh_asset.h"

namespace Blunder {

class AssetManager;

enum class MeshPreviewLoadSource {
  None,
  Final,
  Intermediate,
};

/// Skinned meshes use bind-pose in this slice; non-skinned use mesh rest geometry.
enum class MeshPreviewPoseMode {
  RestPose,
  BindPose,
};

struct MeshPreviewRenderRequest {
  eastl::string mesh_virtual_path;
  uint32_t width{128};
  uint32_t height{128};
  float framing_padding{1.15f};
  MeshPreviewPoseMode pose_mode{MeshPreviewPoseMode::BindPose};
};

struct MeshPreviewRenderResult {
  bool ok{false};
  eastl::string error;
  MeshPreviewLoadSource load_source{MeshPreviewLoadSource::None};
  MeshPreviewPoseMode pose_mode{MeshPreviewPoseMode::RestPose};
  MeshPreviewCameraFrame framing{};
  MeshPreviewStudioLights studio_lights{};
  eastl::vector<uint8_t> rgba;
  uint32_t width{0};
  uint32_t height{0};
};

using MeshPreviewSuccessFn = void (*)(const MeshPreviewRenderResult& result,
                                      void* user_data);
using MeshPreviewFailureFn = void (*)(const eastl::string& error,
                                      void* user_data);

/// Resolve which Pull path was used for a loaded mesh.
inline MeshPreviewLoadSource resolveMeshPreviewLoadSource(
    const MeshAsset& mesh) {
  if (mesh.isFromCookedFinal()) {
    return MeshPreviewLoadSource::Final;
  }
  return MeshPreviewLoadSource::Intermediate;
}

/// Skinned meshes always bind-pose; non-skinned ignore bind-pose request.
inline MeshPreviewPoseMode resolveMeshPreviewPoseMode(
    const MeshAsset& mesh, MeshPreviewPoseMode requested) {
  if (mesh.isSkinned()) {
    return MeshPreviewPoseMode::BindPose;
  }
  return requested == MeshPreviewPoseMode::BindPose
             ? MeshPreviewPoseMode::RestPose
             : requested;
}

/// GPU draw/readback backend (task 1.2/1.3). Optional for API-only callers.
class IMeshPreviewRenderBackend {
 public:
  virtual ~IMeshPreviewRenderBackend() = default;

  virtual bool renderMeshPreview(const MeshAsset& mesh,
                                 const MeshPreviewRenderRequest& request,
                                 const MeshPreviewCameraFrame& framing,
                                 const MeshPreviewStudioLights& lights,
                                 MeshPreviewPoseMode pose_mode,
                                 eastl::vector<uint8_t>& out_rgba) = 0;
};

struct MeshPreviewRenderServiceInit {
  AssetManager* asset_manager{nullptr};
  IMeshPreviewRenderBackend* backend{nullptr};
  MeshPreviewSuccessFn on_success{nullptr};
  MeshPreviewFailureFn on_failure{nullptr};
  void* callback_user{nullptr};
};

/// Shared Mesh Preview Render service for thumbnails and Asset Inspector.
class MeshPreviewRenderService final {
 public:
  MeshPreviewRenderService() = default;

  void initialize(const MeshPreviewRenderServiceInit& init);
  void shutdown();

  /// Load mesh (Final preferred, else Fast Path Intermediate), compute framing
  /// and studio lights. GPU draw is stubbed until task 1.3 when no backend.
  MeshPreviewRenderResult renderMeshAsset(
      const eastl::string& mesh_virtual_path,
      const MeshPreviewRenderRequest& request = {});

 private:
  void notifyFailure(const eastl::string& error);
  void notifySuccess(const MeshPreviewRenderResult& result);

  AssetManager* m_asset_manager{nullptr};
  IMeshPreviewRenderBackend* m_backend{nullptr};
  MeshPreviewSuccessFn m_on_success{nullptr};
  MeshPreviewFailureFn m_on_failure{nullptr};
  void* m_callback_user{nullptr};
  bool m_is_initialized{false};
};

}  // namespace Blunder
