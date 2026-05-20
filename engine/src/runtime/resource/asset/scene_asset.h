#pragma once

#include "EASTL/shared_ptr.h"

#include "runtime/function/scene/scene.h"
#include "runtime/resource/asset/asset.h"

namespace Blunder {

/// CPU-side scene asset (static entity definitions and child scene references).
class SceneAsset final : public Asset {
 public:
  SceneAsset(Asset::Meta meta, Scene scene)
      : Asset(Asset::Type::Scene, eastl::move(meta)),
        m_scene(eastl::move(scene)) {
    setState(State::Loaded);
  }

  const Scene& getScene() const { return m_scene; }
  Scene& getScene() { return m_scene; }

 private:
  Scene m_scene;
};

}  // namespace Blunder
