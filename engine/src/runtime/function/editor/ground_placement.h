#pragma once

#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"

namespace Blunder {

/// Spawn / Placement Preview pose: camera ray ∩ world Z=0; miss → origin.
Vec3 groundPlacementFromRay(const Ray& ray);

/// Slint logical pointer → Ground placement via the Editor Camera.
Vec3 groundPlacementFromWindow(float logical_x, float logical_y);

}  // namespace Blunder
