#include "runtime/function/render/gizmo/gizmo_sdf_aa.h"

#include <algorithm>
#include <cmath>

namespace Blunder {

float sdSegment2d(const float px, const float py, const float ax, const float ay,
                  const float bx, const float by) {
  const float pax = px - ax;
  const float pay = py - ay;
  const float bax = bx - ax;
  const float bay = by - ay;
  const float denom = std::max(bax * bax + bay * bay, 1e-6f);
  const float h = std::clamp((pax * bax + pay * bay) / denom, 0.0f, 1.0f);
  const float dx = pax - bax * h;
  const float dy = pay - bay * h;
  return std::sqrt(dx * dx + dy * dy);
}

float discFillAlpha(const float signed_px_dist, const float edge_px) {
  return std::clamp(edge_px - signed_px_dist, 0.0f, 1.0f);
}

float segmentLineAlpha(const float px, const float py, const float ax, const float ay,
                       const float bx, const float by, const float half_width_px,
                       const float units_per_pixel) {
  const float sdf = sdSegment2d(px, py, ax, ay, bx, by);
  const float signed_px_dist = (sdf - half_width_px) / std::max(units_per_pixel, 1e-6f);
  const float a_core = discFillAlpha(signed_px_dist, 1.25f);
  const float a_fw = discFillAlpha(signed_px_dist, 1.0f);
  return std::max(a_core, a_fw);
}

float sdBox2d(const float px, const float py, const float half_w, const float half_h) {
  const float dx = std::fabs(px) - half_w;
  const float dy = std::fabs(py) - half_h;
  const float ox = std::max(dx, 0.0f);
  const float oy = std::max(dy, 0.0f);
  return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0f);
}

float rectRingAlpha(const float px, const float py, const float half_w, const float half_h,
                    const float half_width_px, const float units_per_pixel) {
  const float sdf = std::fabs(sdBox2d(px, py, half_w, half_h));
  const float half_w_px = std::max(half_width_px, 0.75f);
  const float px_dist = sdf / std::max(units_per_pixel, 1e-6f);
  const float a_core = std::clamp(half_w_px + 1.25f - px_dist, 0.0f, 1.0f);
  const float a_fw = std::clamp(half_w_px + 1.0f - px_dist, 0.0f, 1.0f);
  return std::max(a_core, a_fw);
}

float rectFillAlpha(const float px, const float py, const float half_w, const float half_h,
                    const float units_per_pixel, const float edge_px) {
  const float sdf = sdBox2d(px, py, half_w, half_h);
  const float signed_px_dist = sdf / std::max(units_per_pixel, 1e-6f);
  return discFillAlpha(signed_px_dist, edge_px);
}

}  // namespace Blunder
