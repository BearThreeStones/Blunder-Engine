#pragma once

#include <cstdint>

#include <glm/vec2.hpp>

namespace Blunder {

using DockId = uint32_t;

inline constexpr DockId k_invalid_dock_id = 0;

enum class DockSlot : uint8_t {
  none = 0,
  left,
  right,
  top,
  bottom,
  center,
};

enum class SplitDirection : uint8_t {
  none = 0,
  horizontal,
  vertical,
};

enum class DockNodeKind : uint8_t {
  container,
  split,
  floating,
};

enum class DockPanelKind : uint8_t {
  custom = 0,
  viewport,
  hierarchy,
  inspector,
  content_browser,
};

enum class DockDragState : uint8_t {
  idle = 0,
  dragging,
  hovering_guide,
  dropped,
};

struct DockRect {
  float x{0.0f};
  float y{0.0f};
  float width{0.0f};
  float height{0.0f};

  float right() const { return x + width; }
  float bottom() const { return y + height; }
  glm::vec2 center() const { return glm::vec2{x + width * 0.5f, y + height * 0.5f}; }
  bool valid() const { return width > 0.0f && height > 0.0f; }

  bool contains(const glm::vec2& point) const {
    return point.x >= x && point.x < x + width && point.y >= y &&
           point.y < y + height;
  }
};

inline DockRect makeDockRect(float x, float y, float width, float height) {
  return DockRect{x, y, width, height};
}

inline bool isHorizontalSlot(DockSlot slot) {
  return slot == DockSlot::left || slot == DockSlot::right;
}

inline bool isLeadingSlot(DockSlot slot) {
  return slot == DockSlot::left || slot == DockSlot::top;
}

}
