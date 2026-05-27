#include "runtime/function/render/overlay/origins_overlay.h"

#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"

namespace Blunder {

void OriginsOverlay::begin_sync(OverlayResources& /*res*/,
                                const OverlayState& /*state*/) {
  enabled_ = false;  // TODO: implement
}

void OriginsOverlay::draw_color_only(VkCommandBuffer /*cmd*/,
                                     const OverlayState& /*state*/) {
  if (!enabled_) {
    return;
  }
}

}  // namespace Blunder
