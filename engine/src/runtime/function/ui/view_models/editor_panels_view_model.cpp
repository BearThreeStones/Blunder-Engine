#include "runtime/function/ui/view_models/editor_panels_view_model.h"

namespace Blunder {

void EditorPanelsViewModel::markDirty(const EditorPanelDirty panel) {
  m_dirty_mask |= static_cast<uint8_t>(panel);
}

bool EditorPanelsViewModel::consumeDirty(const EditorPanelDirty panel) {
  const uint8_t flag = static_cast<uint8_t>(panel);
  if ((m_dirty_mask & flag) == 0) {
    return false;
  }
  m_dirty_mask &= static_cast<uint8_t>(~flag);
  return true;
}

}  // namespace Blunder
