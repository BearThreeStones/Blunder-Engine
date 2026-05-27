#include "runtime/core/math/coordinate_system.h"

namespace Blunder {

Mat3 basisGltfToEngine3() {
  // Columns: C*+X, C*+Y, C*+Z  =>  (x,y,z)_gltf -> (x, z, -y)_engine
  return Mat3(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f),
              Vec3(0.0f, -1.0f, 0.0f));
}

Mat4 basisGltfToEngine() {
  const Mat3 rotation = basisGltfToEngine3();
  Mat4 result(1.0f);
  result[0] = Vec4(rotation[0], 0.0f);
  result[1] = Vec4(rotation[1], 0.0f);
  result[2] = Vec4(rotation[2], 0.0f);
  return result;
}

Mat3 basisEngineToGltf3() {
  return Mat3(Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f),
              Vec3(0.0f, 1.0f, 0.0f));
}

Vec3 transformPointGltfToEngine(const Vec3& point_gltf) {
  return basisGltfToEngine3() * point_gltf;
}

Vec3 transformDirectionGltfToEngine(const Vec3& direction_gltf) {
  return basisGltfToEngine3() * direction_gltf;
}

Mat4 similarityGltfToEngine(const Mat4& matrix_gltf) {
  const Mat4 c = basisGltfToEngine();
  const Mat4 c_inv = glm::inverse(c);
  return c * matrix_gltf * c_inv;
}

}  // namespace Blunder
