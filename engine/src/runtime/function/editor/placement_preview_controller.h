#pragma once

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"

namespace Blunder {

class AssetManager;
class MeshAsset;
class RenderSystem;

/// Transient follow-mesh while dragging a Mesh Asset over the editor viewport.
class PlacementPreviewController final {
 public:
  void setSourcePath(eastl::string virtual_path);
  void setPointerOverViewport(bool over_viewport);
  void setGroundPosition(const Vec3& position);
  void clear();

  bool isVisible() const;
  const eastl::string& sourcePath() const { return m_source_path; }
  const Vec3& groundPosition() const { return m_ground_position; }

  void ensureLoaded(AssetManager& asset_manager);
  void submitToRender(RenderSystem* render_system) const;

 private:
  eastl::string m_source_path;
  eastl::string m_loaded_path;
  Vec3 m_ground_position{0.0f};
  bool m_over_viewport{false};
  eastl::shared_ptr<MeshAsset> m_mesh;
};

}  // namespace Blunder
