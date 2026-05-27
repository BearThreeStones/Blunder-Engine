#include "runtime/function/render/overlay/axes_overlay.h"

#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"

namespace Blunder {

void AxesOverlay::begin_sync(OverlayResources& /*res*/,
                             const OverlayState& /*state*/) {
  enabled_ = false;  // TODO: implement
}

void AxesOverlay::draw_line(VkCommandBuffer /*cmd*/,
                            const OverlayState& /*state*/) {
  if (!enabled_) {
    return;
  }
}

}  // namespace Blunder
