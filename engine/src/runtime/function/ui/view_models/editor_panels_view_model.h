#pragma once

#include <cstdint>

namespace Blunder {

enum class EditorPanelDirty : uint8_t {
  hierarchy = 1u << 0,
  inspector = 1u << 1,
  content_browser = 1u << 2,
};

class EditorPanelsViewModel final {
 public:
  void markDirty(EditorPanelDirty panel);
  bool consumeDirty(EditorPanelDirty panel);
  bool anyDirty() const { return m_dirty_mask != 0; }

 private:
  uint8_t m_dirty_mask{0};
};

}  // namespace Blunder
