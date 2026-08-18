#include "runtime/function/editor/hierarchy_line.h"

#include <cassert>
#include <cstdint>

namespace {

void lastSiblingFromIndex() {
  assert(!Blunder::isLastSibling(0, 0));
  assert(Blunder::isLastSibling(0, 1));
  assert(!Blunder::isLastSibling(0, 2));
  assert(Blunder::isLastSibling(1, 2));
}

void rootChildrenHaveEmptyMask() {
  const uint32_t mask =
      Blunder::childAncestorContMask(/*parent_depth=*/0, /*parent_is_last=*/false,
                                     /*parent_mask=*/0);
  assert(mask == 0u);
}

void lastChildStemDoesNotCrossGrandchildren() {
  // Parent at depth 1 is last among its siblings: grandchildren must not
  // continue the grandparent stem at column 0.
  const uint32_t parent_mask = 0;
  const uint32_t child_mask = Blunder::childAncestorContMask(
      /*parent_depth=*/1, /*parent_is_last=*/true, parent_mask);
  assert(child_mask == 0u);
  assert((child_mask & 1u) == 0u);
}

void nonLastAncestorStemContinuesThroughNestedRows() {
  const uint32_t parent_mask = 0;
  const uint32_t child_mask = Blunder::childAncestorContMask(
      /*parent_depth=*/1, /*parent_is_last=*/false, parent_mask);
  assert((child_mask & 1u) != 0u);
}

void nestedTableMatchesVisibleTreeGrammar() {
  // Map (depth 0)
  // ├─ Dimian (depth 1, not last)
  // │    └─ nested (depth 2, last)
  // └─ ice (depth 1, last)
  //      ├─ childA (depth 2, not last)
  //      └─ childB (depth 2, last)
  const uint32_t map_mask = 0;
  const bool map_is_last = true;

  const uint32_t dimian_mask =
      Blunder::childAncestorContMask(0, map_is_last, map_mask);
  assert(dimian_mask == 0u);
  assert(!Blunder::isLastSibling(0, 2));

  const uint32_t nested_mask =
      Blunder::childAncestorContMask(1, /*dimian last=*/false, dimian_mask);
  assert((nested_mask & 1u) != 0u);
  assert(Blunder::isLastSibling(0, 1));

  const uint32_t ice_mask =
      Blunder::childAncestorContMask(0, map_is_last, map_mask);
  assert(ice_mask == 0u);
  assert(Blunder::isLastSibling(1, 2));

  const uint32_t child_a_mask =
      Blunder::childAncestorContMask(1, /*ice last=*/true, ice_mask);
  assert((child_a_mask & 1u) == 0u);
  assert(!Blunder::isLastSibling(0, 2));

  const uint32_t child_b_mask =
      Blunder::childAncestorContMask(1, /*ice last=*/true, ice_mask);
  assert((child_b_mask & 1u) == 0u);
  assert(Blunder::isLastSibling(1, 2));
}

void deeperNonLastKeepsParentThroughBits() {
  const uint32_t parent_mask = 1u;  // through at column 0
  const uint32_t child_mask = Blunder::childAncestorContMask(
      /*parent_depth=*/2, /*parent_is_last=*/false, parent_mask);
  assert((child_mask & 1u) != 0u);
  assert((child_mask & 2u) != 0u);
}

}  // namespace

int main() {
  lastSiblingFromIndex();
  rootChildrenHaveEmptyMask();
  lastChildStemDoesNotCrossGrandchildren();
  nonLastAncestorStemContinuesThroughNestedRows();
  nestedTableMatchesVisibleTreeGrammar();
  deeperNonLastKeepsParentThroughBits();
  return 0;
}
