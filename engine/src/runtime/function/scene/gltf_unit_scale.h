#pragma once

#include <algorithm>
#include <cmath>

#include "runtime/core/math/math_types.h"

namespace Blunder {

/// Khronos Sponza glTF nodesQ0S uniform scale (cm to metres).
constexpr float kGltfCentimeterToMeterScale = 0.008f;

/// Courtyard Uniques authored in metres stay well under this; glTF-local
/// centimetre translations after / 0.008 are hundreds of units.
constexpr float kMeterSpaceTranslationMaxAbs = 64.0f;

inline bool isUniformScale(const Vec3& scale, float epsilon = 1e-4f) {
  return std::fabs(scale.x - scale.y) <= epsilon &&
         std::fabs(scale.y - scale.z) <= epsilon;
}

inline bool isGltfCentimeterUniformScale(const Vec3& scale) {
  return isUniformScale(scale) && scale.x > 1e-4f && scale.x < 0.05f;
}

inline bool looksLikeMeterSpaceTranslation(const Vec3& local) {
  const float max_abs =
      std::max(std::fabs(local.x), std::max(std::fabs(local.y), std::fabs(local.z)));
  return max_abs <= kMeterSpaceTranslationMaxAbs;
}

/// World metres to local under a centimetre-scaled parent:
/// world = parentScale * local.
inline Vec3 meterTranslationToGltfLocal(const Vec3& world_metres,
                                        float parent_uniform_scale) {
  const float scale = parent_uniform_scale > 1e-8f ? parent_uniform_scale : 1.0f;
  return world_metres / scale;
}

}  // namespace Blunder
