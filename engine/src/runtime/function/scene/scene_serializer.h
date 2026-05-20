#pragma once

#include "EASTL/string.h"

#include "runtime/function/scene/scene.h"

namespace Blunder {

/// Deserializes .scene.asset JSON into static Scene data.
class SceneSerializer final {
 public:
  static bool deserialize(const eastl::string& json_text, Scene& out_scene);
};

}  // namespace Blunder
