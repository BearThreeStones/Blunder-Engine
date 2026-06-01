#include "runtime/function/ui/viewport/viewport_input_layer.h"

#include "runtime/core/event/event.h"
#include "runtime/function/render/render_system.h"

namespace Blunder {

ViewportInputLayer::ViewportInputLayer(eastl::weak_ptr<RenderSystem> render_system)
    : Layer("ViewportInput"), m_render_system(eastl::move(render_system)) {}

void ViewportInputLayer::onEvent(Event& event) {
  if (event.handled) {
    return;
  }
  if (const auto render_system = m_render_system.lock()) {
    render_system->onEvent(event);
  }
}

}  // namespace Blunder
