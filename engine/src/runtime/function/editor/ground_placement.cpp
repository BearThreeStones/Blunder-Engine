#include "runtime/function/editor/ground_placement.h"

namespace Blunder {

Vec3 groundPlacementFromRay(const Ray& ray) {
  const Plane ground =
      Plane::fromPointAndNormal(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
  const std::optional<RayHit> hit = intersect(ray, ground);
  if (!hit.has_value()) {
    return Vec3(0.0f);
  }
  return hit->point;
}

}  // namespace Blunder
