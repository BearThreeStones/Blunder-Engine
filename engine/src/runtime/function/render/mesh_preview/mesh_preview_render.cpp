#include "runtime/function/render/mesh_preview/mesh_preview_render.h"

#include "runtime/core/base/macro.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {

void MeshPreviewRenderService::initialize(
    const MeshPreviewRenderServiceInit& init) {
  ASSERT(init.asset_manager != nullptr);
  m_asset_manager = init.asset_manager;
  m_backend = init.backend;
  m_on_success = init.on_success;
  m_on_failure = init.on_failure;
  m_callback_user = init.callback_user;
  m_is_initialized = true;
}

void MeshPreviewRenderService::shutdown() {
  m_is_initialized = false;
  m_asset_manager = nullptr;
  m_backend = nullptr;
}

void MeshPreviewRenderService::notifyFailure(const eastl::string& error) {
  if (m_on_failure != nullptr) {
    m_on_failure(error, m_callback_user);
  }
}

void MeshPreviewRenderService::notifySuccess(
    const MeshPreviewRenderResult& result) {
  if (m_on_success != nullptr) {
    m_on_success(result, m_callback_user);
  }
}

MeshPreviewRenderResult MeshPreviewRenderService::renderMeshAsset(
    const eastl::string& mesh_virtual_path,
    const MeshPreviewRenderRequest& request) {
  MeshPreviewRenderResult result{};
  result.width = request.width;
  result.height = request.height;
  result.studio_lights = defaultMeshPreviewStudioLights();

  if (!m_is_initialized || m_asset_manager == nullptr) {
    result.error = "MeshPreviewRenderService not initialized";
    notifyFailure(result.error);
    return result;
  }
  if (mesh_virtual_path.empty()) {
    result.error = "Mesh Preview Render: empty mesh path";
    notifyFailure(result.error);
    return result;
  }

  const eastl::shared_ptr<MeshAsset> mesh =
      m_asset_manager->loadMesh(mesh_virtual_path);
  if (!mesh) {
    result.error = eastl::string("Mesh Preview Render: failed to load mesh ")
                       .append(mesh_virtual_path.c_str());
    notifyFailure(result.error);
    return result;
  }

  result.load_source = resolveMeshPreviewLoadSource(*mesh);
  result.pose_mode = resolveMeshPreviewPoseMode(*mesh, request.pose_mode);

  MeshPreviewRenderRequest backend_request = request;
  backend_request.mesh_virtual_path = mesh_virtual_path;

  if (request.override_framing && request.framing_override.ok) {
    result.framing = request.framing_override;
  } else {
    MeshPreviewFramingParams framing_params{};
    framing_params.local_bounds = mesh->getLocalBounds();
    framing_params.padding = request.framing_padding;
    framing_params.aspect =
        request.height > 0
            ? static_cast<float>(request.width) /
                  static_cast<float>(request.height)
            : 1.0f;
    result.framing = computeMeshPreviewCameraFrame(framing_params);
    if (!result.framing.ok) {
      result.error = "Mesh Preview Render: failed to compute camera framing";
      notifyFailure(result.error);
      return result;
    }
  }

  if (m_backend != nullptr) {
    if (!m_backend->renderMeshPreview(*mesh, backend_request, result.framing,
                                      result.studio_lights, result.pose_mode,
                                      result.rgba)) {
      result.error =
          "Mesh Preview Render: GPU backend failed (offscreen draw/readback)";
      notifyFailure(result.error);
      return result;
    }
  }

  result.ok = true;
  notifySuccess(result);
  return result;
}

}  // namespace Blunder
