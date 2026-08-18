#pragma once

#include <cstdint>

namespace Blunder {

inline bool isLastSibling(int32_t child_index, int32_t child_count) {
  return child_count > 0 && child_index == child_count - 1;
}

inline uint32_t childAncestorContMask(int32_t parent_depth, bool parent_is_last,
                                      uint32_t parent_mask) {
  uint32_t mask = parent_mask;
  if (parent_depth >= 1 && !parent_is_last) {
    const int32_t bit = parent_depth - 1;
    if (bit >= 0 && bit < 32) {
      mask |= (uint32_t{1} << bit);
    }
  }
  return mask;
}

}  // namespace Blunder
