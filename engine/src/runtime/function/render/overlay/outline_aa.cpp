#include "runtime/function/render/overlay/outline_aa.h"

#include <algorithm>

namespace Blunder {

float outlineEdgeCoverage(const int neighbor_edge_count) {
  const float w = static_cast<float>(std::clamp(neighbor_edge_count, 0, 8));
  const float t = (w - kOutlineEdgeSmoothMin) /
                  std::max(kOutlineEdgeSmoothMax - kOutlineEdgeSmoothMin, 1e-4f);
  return std::clamp(t, 0.0f, 1.0f);
}

}  // namespace Blunder
