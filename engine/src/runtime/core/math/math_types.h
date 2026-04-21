#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <type_traits>

namespace Blunder {

// Vector types
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;

// Matrix types
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

// Quaternion type
using Quat = glm::quat;

// Static assertions to verify type sizes
static_assert(sizeof(Vec2) == sizeof(float) * 2, "Vec2 size mismatch");
static_assert(sizeof(Vec3) == sizeof(float) * 3, "Vec3 size mismatch");
static_assert(sizeof(Vec4) == sizeof(float) * 4, "Vec4 size mismatch");
static_assert(sizeof(Mat3) == sizeof(float) * 9, "Mat3 size mismatch");
static_assert(sizeof(Mat4) == sizeof(float) * 16, "Mat4 size mismatch");
static_assert(sizeof(Quat) == sizeof(float) * 4, "Quat size mismatch");

// Verify types are trivially copyable for performance
static_assert(std::is_trivially_copyable_v<Vec3>, "Vec3 must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Mat4>, "Mat4 must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Quat>, "Quat must be trivially copyable");

}  // namespace Blunder
