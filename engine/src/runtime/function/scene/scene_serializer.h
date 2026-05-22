#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/scene.h"

namespace Blunder {

/// Serializes / deserializes .scene.asset JSON into static Scene data.
class SceneSerializer final {
 public:
  static bool deserialize(const eastl::string& json_text, Scene& out_scene);
  static bool serialize(const Scene& scene, eastl::string& out_json);

  /// Matches JSON `"rotationMode": "euler_degrees"` (compose order qz * qy * qx).
  static Quat rotationFromEulerDegrees(const Vec3& euler_degrees);
  static Vec3 rotationToEulerDegrees(const Quat& rotation);
};

}  // namespace Blunder
