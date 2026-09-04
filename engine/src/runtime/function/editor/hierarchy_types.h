#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

enum class HierarchyRowIconKind : uint8_t {
  Transform = 0,
  MeshRenderer = 1,
  Camera = 2,
  Light = 3,
  Skeleton = 4,
  AnimationTree = 5,
  Behaviour = 6,
  SkeletonModifier = 7,
};

struct HierarchyRowIconSlot {
  HierarchyRowIconKind kind{HierarchyRowIconKind::Transform};
  int32_t index{0};
};

struct EditorHierarchyTreeRow {
  uint32_t entity_id{0};
  eastl::string display_name;
  int32_t depth{0};
  bool is_expanded{false};
  bool has_children{false};
  bool is_last_sibling{false};
  uint32_t ancestor_cont_mask{0};
  bool object_active{true};
  bool active_in_hierarchy{true};
  eastl::vector<HierarchyRowIconSlot> icons;
};

constexpr float k_hierarchy_row_icon_px = 16.0f;
constexpr float k_hierarchy_row_icon_gap_px = 2.0f;

}  // namespace Blunder
