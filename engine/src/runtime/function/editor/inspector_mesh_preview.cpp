#include "runtime/function/editor/inspector_mesh_preview.h"

#include "runtime/function/render/overlay/camera_preview_rt_size.h"

namespace Blunder {

namespace {

constexpr float kInspectorPreviewLogicalW = 320.0f;
constexpr float kInspectorPreviewLogicalH = 200.0f;

CameraPreviewRtSize inspectorPreviewRtSize() {
  return computeCameraPreviewRtSize(kInspectorPreviewLogicalW,
                                    kInspectorPreviewLogicalH);
}

}  // namespace

void InspectorMeshPreview::clear() {
  m_mesh_virtual_path.clear();
  m_orbit_camera.clear();
  m_rgba.clear();
  m_width = 0;
  m_height = 0;
  m_has_image = false;
  m_dirty = false;
}

void InspectorMeshPreview::bindMesh(const eastl::string& mesh_virtual_path) {
  if (m_mesh_virtual_path == mesh_virtual_path) {
    return;
  }
  m_mesh_virtual_path = mesh_virtual_path;
  m_orbit_camera.clear();
  m_rgba.clear();
  m_width = 0;
  m_height = 0;
  m_has_image = false;
  m_dirty = !mesh_virtual_path.empty();
}

void InspectorMeshPreview::orbit(float delta_x, float delta_y) {
  if (!hasActiveMesh()) {
    return;
  }
  m_orbit_camera.orbit(delta_x, delta_y);
  m_dirty = true;
}

void InspectorMeshPreview::zoom(float wheel_delta) {
  if (!hasActiveMesh()) {
    return;
  }
  m_orbit_camera.zoom(wheel_delta);
  m_dirty = true;
}

void InspectorMeshPreview::resetView() {
  if (!hasActiveMesh()) {
    return;
  }
  m_orbit_camera.reset();
  m_dirty = true;
}

bool InspectorMeshPreview::tick(MeshPreviewRenderService* service) {
  if (!m_dirty || service == nullptr || !hasActiveMesh()) {
    return false;
  }
  return renderFrame(service);
}

bool InspectorMeshPreview::renderFrame(MeshPreviewRenderService* service) {
  const CameraPreviewRtSize rt_size = inspectorPreviewRtSize();
  if (!rt_size.ok) {
    return false;
  }

  MeshPreviewRenderRequest request{};
  request.mesh_virtual_path = m_mesh_virtual_path;
  request.width = rt_size.width;
  request.height = rt_size.height;

  MeshPreviewRenderResult bootstrap{};
  if (!m_orbit_camera.defaultFrame().ok) {
    bootstrap = service->renderMeshAsset(m_mesh_virtual_path, request);
    if (!bootstrap.ok) {
      m_has_image = false;
      m_dirty = false;
      return false;
    }
    m_orbit_camera.setDefaultFrame(bootstrap.framing);
  }

  const MeshPreviewCameraFrame framing = m_orbit_camera.currentFrame();
  if (!framing.ok) {
    m_has_image = false;
    m_dirty = false;
    return false;
  }

  request.override_framing = true;
  request.framing_override = framing;
  const MeshPreviewRenderResult result =
      service->renderMeshAsset(m_mesh_virtual_path, request);
  m_dirty = false;
  if (!result.ok || result.rgba.empty()) {
    m_has_image = false;
    return false;
  }

  m_rgba = eastl::move(result.rgba);
  m_width = result.width;
  m_height = result.height;
  m_has_image = true;
  return true;
}

}  // namespace Blunder
