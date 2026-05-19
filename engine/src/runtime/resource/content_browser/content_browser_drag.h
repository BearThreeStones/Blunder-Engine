#pragma once

#include <cstdint>

#include "EASTL/string.h"

namespace Blunder {

/// Pointer-driven drag state for the content browser (Slint has no DragArea in our fork).
class ContentBrowserDragController final {
 public:
  void reset();
  void beginPress(const eastl::string& virtual_path, float x, float y);
  void updateMove(float x, float y);
  void endPress();

  bool isDragging() const { return m_is_dragging; }
  const eastl::string& sourcePath() const { return m_source_path; }

 private:
  static constexpr float k_drag_threshold_px = 5.0f;

  eastl::string m_source_path;
  float m_press_x{0.0f};
  float m_press_y{0.0f};
  bool m_press_active{false};
  bool m_is_dragging{false};
};

}  // namespace Blunder
