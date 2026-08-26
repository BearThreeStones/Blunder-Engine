#pragma once

#include <cstdint>

#include "EASTL/vector.h"

#include "runtime/function/render/mesh_preview/mesh_preview_draw_builder.h"
#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/mesh_preview/mesh_preview_studio_lights.h"

namespace Blunder {

class SceneInstance;

/// Dedicated-offscreen GPU draw + CPU readback used by Scene stills.
class ISceneStillGpuBackend {
 public:
  virtual ~ISceneStillGpuBackend() = default;

  virtual bool renderSubmeshDraws(
      const eastl::vector<MeshPreviewSubmeshDraw>& draws,
      const MeshPreviewCameraFrame& framing,
      const MeshPreviewStudioLights& lights, uint32_t width, uint32_t height,
      eastl::vector<uint8_t>& out_rgba,
      const SceneInstance* lighting_scene = nullptr) = 0;
};

}  // namespace Blunder
