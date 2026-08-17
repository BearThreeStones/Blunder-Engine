#pragma once

#include "EASTL/string.h"

namespace Blunder {

/// Leftover JSON property on a Missing SkeletonModifier (raw value token).
struct SkeletonModifierExtraField final {
  eastl::string key;
  eastl::string json_value;
};

}  // namespace Blunder
