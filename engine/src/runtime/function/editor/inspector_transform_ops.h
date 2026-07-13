#pragma once

#include "runtime/core/math/math_types.h"

namespace Blunder {

/// Proportional scale-link edit. `axis` is 0=X,1=Y,2=Z. If |old[axis]| < eps,
/// only that axis is set to `new_value`.
Vec3 applyScaleLinkEdit(const Vec3& old_scale, int axis, float new_value,
                        float eps = 1e-6f);

bool isMixedComponent(float a, float b, float eps = 1e-5f);

void applyAbsoluteComponent(float value, float& component);
void applyDeltaComponent(float delta, float& component);

/// Apply Euler-degree delta through SceneSerializer compose/decompose.
Quat applyEulerDeltaDegrees(const Quat& current, const Vec3& delta_degrees);

Quat normalizeQuaternion(const Quat& q);

}  // namespace Blunder
