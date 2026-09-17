#include "runtime/function/scene/entity.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {

void Entity::addGroup(const eastl::string& name) {
  if (name.empty()) {
    return;
  }
  if (std::find(m_groups.begin(), m_groups.end(), name) != m_groups.end()) {
    return;
  }
  m_groups.push_back(name);
}

void Entity::removeGroup(const eastl::string& name) {
  m_groups.erase(std::remove(m_groups.begin(), m_groups.end(), name), m_groups.end());
}

bool Entity::isInGroup(const eastl::string& name) const {
  return std::find(m_groups.begin(), m_groups.end(), name) != m_groups.end();
}

Mat4 Entity::getLocalMatrix() const {
  const Mat4 translation = glm::translate(Mat4(1.0f), m_position);
  const Mat4 rotation = glm::mat4_cast(m_rotation);
  const Mat4 scale = glm::scale(Mat4(1.0f), m_scale);
  return translation * rotation * scale;
}

}  // namespace Blunder
