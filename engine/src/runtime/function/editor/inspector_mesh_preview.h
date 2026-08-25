#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/render/mesh_preview/mesh_preview_orbit_camera.h"
#include "runtime/function/render/mesh_preview/mesh_preview_render.h"

namespace Blunder {

class MeshPreviewRenderService;

/// Asset Inspector interactive Mesh Preview (ephemeral orbit + RT readback).
class InspectorMeshPreview final {
 public:
  void clear();
  void bindMesh(const eastl::string& mesh_virtual_path);

  void orbit(float delta_x, float delta_y);
  void zoom(float wheel_delta);
  void resetView();

  bool hasActiveMesh() const { return !m_mesh_virtual_path.empty(); }
  bool hasRenderableImage() const { return m_has_image; }
  bool isDirty() const { return m_dirty; }
  bool hasUserOrbit() const { return m_orbit_camera.hasUserOrbit(); }

  const eastl::vector<uint8_t>& rgba() const { return m_rgba; }
  uint32_t width() const { return m_width; }
  uint32_t height() const { return m_height; }

  /// Renders when dirty; returns true when pixels were updated.
  bool tick(MeshPreviewRenderService* service);

  void markDirty() { m_dirty = hasActiveMesh(); }

 private:
  bool renderFrame(MeshPreviewRenderService* service);

  eastl::string m_mesh_virtual_path;
  MeshPreviewOrbitCamera m_orbit_camera;
  eastl::vector<uint8_t> m_rgba;
  uint32_t m_width{0};
  uint32_t m_height{0};
  bool m_has_image{false};
  bool m_dirty{false};
};

}  // namespace Blunder
