#include "runtime/function/render/gizmo/transform_gizmo_types.h"

#include <algorithm>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/render/gizmo/gizmo_math.h"

namespace Blunder {

namespace {

glm::vec4 withHighlight(const glm::vec3& rgb, const bool highlight) {
  const float boost = highlight ? 1.35f : 1.0f;
  return glm::vec4(std::min(rgb.x * boost, 1.0f), std::min(rgb.y * boost, 1.0f),
                   std::min(rgb.z * boost, 1.0f), 1.0f);
}

}  // namespace

glm::vec4 axisColorFor(const ManipulatorAxis axis, const bool highlight) {
  switch (axis) {
    case ManipulatorAxis::trans_x:
    case ManipulatorAxis::rot_x:
      return withHighlight(kAxisColorPositiveX, highlight);
    case ManipulatorAxis::trans_y:
    case ManipulatorAxis::rot_y:
      return withHighlight(kAxisColorPositiveY, highlight);
    case ManipulatorAxis::trans_z:
    case ManipulatorAxis::rot_z:
      return withHighlight(kAxisColorPositiveZ, highlight);
    case ManipulatorAxis::trans_xy:
      return withHighlight(
          glm::normalize(kAxisColorPositiveX + kAxisColorPositiveY), highlight);
    case ManipulatorAxis::trans_yz:
      return withHighlight(
          glm::normalize(kAxisColorPositiveY + kAxisColorPositiveZ), highlight);
    case ManipulatorAxis::trans_zx:
      return withHighlight(
          glm::normalize(kAxisColorPositiveZ + kAxisColorPositiveX), highlight);
    default:
      return centerColor(highlight);
  }
}

glm::vec4 centerColor(const bool highlight) {
  const float v = highlight ? 1.0f : 0.92f;
  return glm::vec4(v, v, v, 1.0f);
}

glm::vec3 manipulatorLocalDirection(const ManipulatorAxis axis) {
  switch (axis) {
    case ManipulatorAxis::trans_x:
      return glm::vec3(1.0f, 0.0f, 0.0f);
    case ManipulatorAxis::trans_y:
      return glm::vec3(0.0f, 1.0f, 0.0f);
    case ManipulatorAxis::trans_z:
      return glm::vec3(0.0f, 0.0f, 1.0f);
    default:
      return glm::vec3(0.0f);
  }
}

void manipulatorTranslationMask(const ManipulatorAxis axis, bool out_mask[3]) {
  out_mask[0] = out_mask[1] = out_mask[2] = false;
  switch (axis) {
    case ManipulatorAxis::trans_x:
      out_mask[0] = true;
      break;
    case ManipulatorAxis::trans_y:
      out_mask[1] = true;
      break;
    case ManipulatorAxis::trans_z:
      out_mask[2] = true;
      break;
    case ManipulatorAxis::trans_xy:
      out_mask[0] = out_mask[1] = true;
      break;
    case ManipulatorAxis::trans_yz:
      out_mask[1] = out_mask[2] = true;
      break;
    case ManipulatorAxis::trans_zx:
      out_mask[2] = out_mask[0] = true;
      break;
    case ManipulatorAxis::trans_c:
      out_mask[0] = out_mask[1] = out_mask[2] = true;
      break;
    default:
      break;
  }
}

bool isTranslationManipulator(const ManipulatorAxis axis) {
  return axis >= ManipulatorAxis::trans_x && axis <= ManipulatorAxis::trans_c;
}

bool isRotationManipulator(const ManipulatorAxis axis) {
  return axis >= ManipulatorAxis::rot_x && axis <= ManipulatorAxis::rot_z;
}

glm::vec3 rotationAxisFor(const ManipulatorAxis axis, const GizmoBasis& basis) {
  switch (axis) {
    case ManipulatorAxis::rot_x:
      return basis.axis_x;
    case ManipulatorAxis::rot_y:
      return basis.axis_y;
    case ManipulatorAxis::rot_z:
      return basis.axis_z;
    default:
      return basis.axis_z;
  }
}

}  // namespace Blunder
