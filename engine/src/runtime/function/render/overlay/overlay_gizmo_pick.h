#pragma once

#include <optional>

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

struct OverlayGizmoPickHit {
  EntityId entity_id{k_invalid_entity_id};
  float view_depth{0.0f};
};

/// Camera overlay stores view-space Z. Camera looks along -Z, so a larger value
/// is closer to the viewer.
inline bool overlayGizmoViewDepthIsCloser(float candidate, float current_best) {
  return candidate > current_best;
}

inline std::optional<OverlayGizmoPickHit> pickCloserOverlayGizmoHit(
    const std::optional<OverlayGizmoPickHit>& a,
    const std::optional<OverlayGizmoPickHit>& b) {
  if (!a.has_value()) {
    return b;
  }
  if (!b.has_value()) {
    return a;
  }
  if (overlayGizmoViewDepthIsCloser(b->view_depth, a->view_depth)) {
    return b;
  }
  return a;
}

}  // namespace Blunder
