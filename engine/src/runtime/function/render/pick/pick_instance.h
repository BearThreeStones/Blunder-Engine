#pragma once

#include <cstdint>

namespace Blunder {

enum PickInstanceFlags : uint32_t {
  k_pick_instance_pickable = 1u << 0,
};

/// std430-friendly row for GPU broad phase (P2) and metadata (P1).
struct PickInstanceGpu {
  uint32_t entity_id{0};
  uint32_t parent_id{0};
  uint32_t flags{0};
  uint32_t draw_index{0};
  float aabb_min[3]{0.0f, 0.0f, 0.0f};
  float aabb_pad0{0.0f};
  float aabb_max[3]{0.0f, 0.0f, 0.0f};
  float aabb_pad1{0.0f};
};

static_assert(sizeof(PickInstanceGpu) == 48, "PickInstanceGpu size must match shader");

}  // namespace Blunder
