#pragma once

#include <cstdint>

#include "EASTL/string.h"

namespace Blunder {

struct EditorHierarchyTreeRow {
  uint32_t entity_id{0};
  eastl::string display_name;
  int32_t depth{0};
  bool is_expanded{false};
  bool has_children{false};
};

}  // namespace Blunder
