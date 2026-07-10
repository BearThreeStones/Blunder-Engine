#include "runtime/function/render/overlay/overlay_line_aa.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

namespace Blunder {

namespace {

float smoothstep(const float edge0, const float edge1, const float x) {
  const float t =
      std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-6f), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

LineData decodeLineData(const glm::vec2 packed) {
  const float dist = (packed.y - 0.5f) * 2.5f;
  const float sin_theta = (packed.x - 0.5f) * 2.0f;
  const float cos_theta = std::sqrt(std::max(1.0f - sin_theta * sin_theta, 0.0f));
  glm::vec2 perp{sin_theta, cos_theta};
  const float len = glm::length(perp);
  if (len > 1e-6f) {
    perp /= len;
  }
  return LineData{perp, dist, packed.y};
}

float lineCoverage(const float distance_to_line, const float line_kernel_size,
                   const bool smooth) {
  const float x = std::fabs(distance_to_line) - line_kernel_size;
  if (smooth) {
    // Blender: smoothstep(LINE_SMOOTH_END, LINE_SMOOTH_START, x) — inverted edge order.
    return 1.0f - smoothstep(kLineSmoothStart, kLineSmoothEnd, x);
  }
  return (line_kernel_size - std::fabs(distance_to_line)) >= -0.5f ? 1.0f : 0.0f;
}

float neighborLineDist(const LineData& neighbor, const glm::ivec2 offset) {
  const bool is_dir_horizontal =
      std::fabs(neighbor.dir.x) > std::fabs(neighbor.dir.y);
  const bool is_ofs_horizontal = offset.x != 0;
  if (!neighbor.is_valid() || is_ofs_horizontal != is_dir_horizontal) {
    return 1e10f;
  }
  return neighbor.dist +
         static_cast<float>(offset.x) * (-neighbor.dir.x) +
         static_cast<float>(offset.y) * (-neighbor.dir.y);
}

}  // namespace Blunder
