#pragma once

#include "EASTL/weak_ptr.h"

#include "runtime/core/layer/layer.h"

namespace Blunder {

class RenderSystem;

/// Routes editor viewport input (camera, gizmo) through the LayerStack.
/// Holds a weak reference so a destroyed RenderSystem cannot be dereferenced.
class ViewportInputLayer final : public Layer {
 public:
  explicit ViewportInputLayer(eastl::weak_ptr<RenderSystem> render_system);

  void onEvent(Event& event) override;

 private:
  eastl::weak_ptr<RenderSystem> m_render_system;
};

}  // namespace Blunder
