#pragma once

#include "runtime/core/math/math_types.h"

#include <algorithm>

#include <glm/glm.hpp>

namespace Blunder {

struct FogComponent final {
  bool enabled{true};
  bool volumetric_enabled{true};
  float density{0.02f};
  float height_falloff{0.2f};
  float view_distance{60.0f};
  Vec3 albedo{1.0f, 1.0f, 1.0f};
  float scattering_g{0.2f};
};

inline void sanitizeFogComponent(FogComponent& fog) {
  fog.density = std::max(fog.density, 0.0f);
  fog.height_falloff = std::max(fog.height_falloff, 0.0f);
  fog.view_distance = std::max(fog.view_distance, 1e-3f);
  fog.scattering_g = glm::clamp(fog.scattering_g, -0.99f, 0.99f);
}

inline bool fogComponentsEqual(const FogComponent& a, const FogComponent& b) {
  return a.enabled == b.enabled && a.volumetric_enabled == b.volumetric_enabled &&
         a.density == b.density && a.height_falloff == b.height_falloff &&
         a.view_distance == b.view_distance && a.albedo == b.albedo &&
         a.scattering_g == b.scattering_g;
}

}  // namespace Blunder
