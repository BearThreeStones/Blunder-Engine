#pragma once

#include "runtime/core/math/math_types.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {

/// Transform class representing position, rotation, and scale (TRS)
/// Supports parent-child hierarchy with raw pointer (user manages lifetime)
class Transform final {
 public:
  Transform() = default;
  
  Transform(const Vec3& position, const Quat& rotation, const Vec3& scale)
      : m_position(position), m_rotation(rotation), m_scale(scale) {}

  // Position accessors
  const Vec3& getPosition() const { return m_position; }
  void setPosition(const Vec3& position) { m_position = position; }
  void translate(const Vec3& delta) { m_position += delta; }

  // Rotation accessors
  const Quat& getRotation() const { return m_rotation; }
  void setRotation(const Quat& rotation) { m_rotation = rotation; }
  void rotate(const Quat& delta) { m_rotation = delta * m_rotation; }
  
  void rotate(const Vec3& axis, float angleRadians) {
    m_rotation = glm::rotate(m_rotation, angleRadians, axis);
  }

  // Scale accessors
  const Vec3& getScale() const { return m_scale; }
  void setScale(const Vec3& scale) { m_scale = scale; }
  void setUniformScale(float scale) { m_scale = Vec3(scale); }

  // Parent hierarchy
  Transform* getParent() const { return m_parent; }
  void setParent(Transform* parent) { m_parent = parent; }

  /// Compute local transformation matrix (T * R * S)
  Mat4 getLocalMatrix() const {
    Mat4 T = glm::translate(Mat4(1.0f), m_position);
    Mat4 R = glm::mat4_cast(m_rotation);
    Mat4 S = glm::scale(Mat4(1.0f), m_scale);
    return T * R * S;
  }

  /// Compute world transformation matrix (includes parent hierarchy)
  Mat4 getWorldMatrix() const {
    Mat4 local = getLocalMatrix();
    if (m_parent != nullptr) {
      return m_parent->getWorldMatrix() * local;
    }
    return local;
  }

  /// Get local forward direction (negative Z in local space)
  Vec3 getForward() const {
    return glm::normalize(m_rotation * Vec3(0.0f, 0.0f, -1.0f));
  }

  /// Get local right direction (positive X in local space)
  Vec3 getRight() const {
    return glm::normalize(m_rotation * Vec3(1.0f, 0.0f, 0.0f));
  }

  /// Get local up direction (positive Y in local space)
  Vec3 getUp() const {
    return glm::normalize(m_rotation * Vec3(0.0f, 1.0f, 0.0f));
  }

  /// Transform a point from local space to world space
  Vec3 transformPoint(const Vec3& localPoint) const {
    Vec4 result = getWorldMatrix() * Vec4(localPoint, 1.0f);
    return Vec3(result);
  }

  /// Transform a direction from local space to world space (ignores translation)
  Vec3 transformVector(const Vec3& localVec) const {
    Vec4 result = getWorldMatrix() * Vec4(localVec, 0.0f);
    return Vec3(result);
  }

  /// Look at a target position
  void lookAt(const Vec3& target, const Vec3& up = Vec3(0.0f, 1.0f, 0.0f)) {
    Vec3 direction = glm::normalize(target - m_position);
    m_rotation = glm::quatLookAt(direction, up);
  }

 private:
  Vec3 m_position{0.0f, 0.0f, 0.0f};
  Quat m_rotation{glm::identity<Quat>()};
  Vec3 m_scale{1.0f, 1.0f, 1.0f};
  Transform* m_parent{nullptr};
};

}  // namespace Blunder
