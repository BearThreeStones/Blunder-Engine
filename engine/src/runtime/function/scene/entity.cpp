#include "runtime/function/scene/entity.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {

Mat4 Entity::getLocalMatrix() const {
  const Mat4 translation = glm::translate(Mat4(1.0f), m_position);
  const Mat4 rotation = glm::mat4_cast(m_rotation);
  const Mat4 scale = glm::scale(Mat4(1.0f), m_scale);
  return translation * rotation * scale;
}

}  // namespace Blunder
