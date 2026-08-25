#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

enum class LightType {
  directional,
  point,
  spot,
  area,
};

enum class LightContribution {
  illuminateAndShadows,
  illuminateOnly,
  shadowsOnly,
};

struct LightComponent final {
  LightType type{LightType::directional};
  Vec3 color{1.0f, 1.0f, 1.0f};
  float intensity{1.0f};
  bool enabled{true};
  LightContribution contribution{LightContribution::illuminateAndShadows};
  float range{10.0f};
  float inner_cone_degrees{0.0f};
  float outer_cone_degrees{45.0f};
  float width{1.0f};
  float height{1.0f};
  eastl::vector<EntityId> linking;
};

inline bool lightContributionIncludesIlluminate(LightContribution contribution) {
  return contribution == LightContribution::illuminateAndShadows ||
         contribution == LightContribution::illuminateOnly;
}

inline bool lightContributionIncludesShadows(LightContribution contribution) {
  return contribution == LightContribution::illuminateAndShadows ||
         contribution == LightContribution::shadowsOnly;
}

inline const char* lightTypeToJson(LightType type) {
  switch (type) {
    case LightType::point:
      return "point";
    case LightType::spot:
      return "spot";
    case LightType::area:
      return "area";
    case LightType::directional:
    default:
      return "directional";
  }
}

inline bool lightTypeFromJson(const eastl::string& value, LightType& out_type) {
  if (value == "point") {
    out_type = LightType::point;
    return true;
  }
  if (value == "spot") {
    out_type = LightType::spot;
    return true;
  }
  if (value == "area") {
    out_type = LightType::area;
    return true;
  }
  if (value == "directional") {
    out_type = LightType::directional;
    return true;
  }
  return false;
}

inline const char* lightContributionToJson(LightContribution contribution) {
  switch (contribution) {
    case LightContribution::illuminateOnly:
      return "illuminateOnly";
    case LightContribution::shadowsOnly:
      return "shadowsOnly";
    case LightContribution::illuminateAndShadows:
    default:
      return "illuminateAndShadows";
  }
}

inline bool lightContributionFromJson(const eastl::string& value,
                                      LightContribution& out_contribution) {
  if (value == "illuminateOnly") {
    out_contribution = LightContribution::illuminateOnly;
    return true;
  }
  if (value == "shadowsOnly") {
    out_contribution = LightContribution::shadowsOnly;
    return true;
  }
  if (value == "illuminateAndShadows") {
    out_contribution = LightContribution::illuminateAndShadows;
    return true;
  }
  return false;
}

}  // namespace Blunder
