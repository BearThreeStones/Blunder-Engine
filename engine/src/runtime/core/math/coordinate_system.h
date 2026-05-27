#pragma once

#include "runtime/core/math/math_types.h"

namespace Blunder {

/// Blunder world space: right-handed, Z-up.
/// +X right, +Y forward, +Z up (X x Y = Z).
inline constexpr Vec3 kWorldRight{1.0f, 0.0f, 0.0f};
inline constexpr Vec3 kWorldForward{0.0f, 1.0f, 0.0f};
inline constexpr Vec3 kWorldUp{0.0f, 0.0f, 1.0f};

/// Positive world axis colors (viewport grid / gizmos, Unity-style RGB = XYZ).
inline constexpr Vec3 kAxisColorPositiveX{0.90f, 0.28f, 0.28f};
inline constexpr Vec3 kAxisColorPositiveY{0.42f, 0.82f, 0.32f};
inline constexpr Vec3 kAxisColorPositiveZ{0.32f, 0.52f, 0.94f};

/// 3x3 rotation C: glTF (RH, Y-up) -> engine (RH, Z-up).
/// Maps (x, y, z)_gltf to (x, z, -y)_engine (rotation -90 deg about +X).
Mat3 basisGltfToEngine3();

/// 4x4 homogeneous form of basisGltfToEngine3().
Mat4 basisGltfToEngine();

/// Inverse basis (engine -> glTF).
Mat3 basisEngineToGltf3();

Vec3 transformPointGltfToEngine(const Vec3& point_gltf);
Vec3 transformDirectionGltfToEngine(const Vec3& direction_gltf);

/// Converts a glTF node/world matrix: T_engine = C * T_gltf * C^-1.
Mat4 similarityGltfToEngine(const Mat4& matrix_gltf);

}  // namespace Blunder
