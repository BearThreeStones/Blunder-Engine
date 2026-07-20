#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/scene.h"

namespace Blunder {

class AssetRegistry;

/// Serializes / deserializes .scene.asset JSON into static Scene data.
class SceneSerializer final {
 public:
  /// When `registry` is non-null, legacy entity `"mesh"` paths are resolved to
  /// GUIDs via `AssetRegistry::findGuidForPath` when possible.
  static bool deserialize(const eastl::string& json_text, Scene& out_scene,
                          const AssetRegistry* registry = nullptr);
  /// Writes top-level `"guid"` when set. When `registry` is non-null, entity
  /// `"mesh"` fields that are still legacy paths are rewritten as GUIDs.
  static bool serialize(const Scene& scene, eastl::string& out_json,
                        const AssetRegistry* registry = nullptr);

  /// Matches JSON `"rotationMode": "euler_degrees"` (compose order qz * qy * qx).
  static Quat rotationFromEulerDegrees(const Vec3& euler_degrees);
  static Vec3 rotationToEulerDegrees(const Quat& rotation);
};

}  // namespace Blunder
