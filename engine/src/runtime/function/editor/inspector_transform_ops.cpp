#include "runtime/function/editor/inspector_transform_ops.h"

#include "runtime/function/scene/scene_serializer.h"

#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace Blunder {

Vec3 applyScaleLinkEdit(const Vec3& old_scale, int axis, float new_value,
                        float eps) {
  Vec3 out = old_scale;
  if (axis < 0 || axis > 2) {
    return out;
  }
  const float old_axis = old_scale[axis];
  if (std::fabs(old_axis) < eps) {
    out[axis] = new_value;
    return out;
  }
  const float factor = new_value / old_axis;
  out = old_scale * factor;
  return out;
}

bool isMixedComponent(float a, float b, float eps) {
  return std::fabs(a - b) > eps;
}

void applyAbsoluteComponent(float value, float& component) {
  component = value;
}

void applyDeltaComponent(float delta, float& component) {
  component += delta;
}

Quat applyEulerDeltaDegrees(const Quat& current, const Vec3& delta_degrees) {
  const Vec3 euler = SceneSerializer::rotationToEulerDegrees(current);
  return SceneSerializer::rotationFromEulerDegrees(euler + delta_degrees);
}

Quat normalizeQuaternion(const Quat& q) {
  const float len = glm::length(q);
  if (len < 1e-8f) {
    return glm::identity<Quat>();
  }
  return q / len;
}

}  // namespace Blunder
