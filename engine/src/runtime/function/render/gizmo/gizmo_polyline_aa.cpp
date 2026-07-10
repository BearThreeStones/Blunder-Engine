#include "runtime/function/render/gizmo/gizmo_polyline_aa.h"

#include <algorithm>
#include <cmath>

namespace Blunder {

float polylineStrokeAlpha(const float stroke_coord, const float fw) {
  const float dist = std::clamp(std::fabs(stroke_coord), 0.0f, 1.0f);
  const float aa = std::max(fw * 1.5f, 1e-4f);
  const float t = (dist - (1.0f - aa)) / aa;
  return 1.0f - std::clamp(t, 0.0f, 1.0f);
}

float polylineStrokeAlphaBlender(const float smoothline, const float line_width,
                                 const bool line_smooth) {
  const float half_w =
      (line_width + (line_smooth ? kPolylineSmoothWidth : 0.0f)) * 0.5f;
  return std::clamp(half_w - std::fabs(smoothline), 0.0f, 1.0f);
}

}  // namespace Blunder
