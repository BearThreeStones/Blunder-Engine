#pragma once

namespace Blunder {

/// Hit-test the Camera Preview panel in viewport-local logical coordinates.
/// Panel rect includes collapsed title-bar height (see camera_preview_panel.slint).
inline bool hitCameraPreviewPanelLocal(float local_x, float local_y, float panel_x,
                                       float panel_y, float panel_w, float panel_h) {
  return local_x >= panel_x && local_x < panel_x + panel_w && local_y >= panel_y &&
         local_y < panel_y + panel_h;
}

}  // namespace Blunder
