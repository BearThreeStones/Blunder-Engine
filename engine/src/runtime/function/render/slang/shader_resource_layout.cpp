#include "runtime/function/render/slang/shader_resource_layout.h"

#include "EASTL/algorithm.h"
#include "EASTL/sort.h"

#include "runtime/core/base/macro.h"

namespace Blunder {

namespace {

struct BindingKey {
  uint32_t set{0};
  uint32_t binding{0};

  bool operator<(const BindingKey& other) const {
    if (set != other.set) {
      return set < other.set;
    }
    return binding < other.binding;
  }

  bool operator==(const BindingKey& other) const {
    return set == other.set && binding == other.binding;
  }
};

uint32_t uniqueSortedKeys(BindingKey* keys, uint32_t count) {
  if (count <= 1) {
    return count;
  }
  eastl::sort(keys, keys + count);
  return static_cast<uint32_t>(eastl::unique(keys, keys + count) - keys);
}

}  // namespace

bool shaderResourceBindingsMatch(const ShaderResourceLayout& layout,
                                 const uint32_t* expected_bindings,
                                 uint32_t expected_count,
                                 const uint32_t* expected_sets) {
  if (layout.count > k_max_expected_descriptor_bindings) {
    return false;
  }
  if (expected_count > k_max_expected_descriptor_bindings) {
    return false;
  }

  BindingKey extracted[k_max_expected_descriptor_bindings];
  for (uint32_t i = 0; i < layout.count; ++i) {
    extracted[i] = {layout.bindings[i].set, layout.bindings[i].binding};
  }
  const uint32_t extracted_count = uniqueSortedKeys(extracted, layout.count);

  BindingKey expected[k_max_expected_descriptor_bindings];
  uint32_t filled_expected = 0;
  if (expected_bindings != nullptr) {
    for (uint32_t i = 0; i < expected_count; ++i) {
      expected[i] = {expected_sets != nullptr ? expected_sets[i] : 0,
                     expected_bindings[i]};
    }
    filled_expected = expected_count;
  }
  const uint32_t unique_expected = uniqueSortedKeys(expected, filled_expected);

  if (extracted_count != unique_expected) {
    return false;
  }
  for (uint32_t i = 0; i < extracted_count; ++i) {
    if (!(extracted[i] == expected[i])) {
      return false;
    }
  }
  return true;
}

void fillSequentialExpectedBindings(uint32_t* bindings, uint32_t* count,
                                    uint32_t n) {
  if (bindings == nullptr || count == nullptr) {
    return;
  }
  if (n > k_max_expected_descriptor_bindings) {
    n = k_max_expected_descriptor_bindings;
  }
  *count = n;
  for (uint32_t i = 0; i < n; ++i) {
    bindings[i] = i;
  }
}

void fillPbrMeshExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                 uint32_t* count, bool skinned) {
  if (bindings == nullptr || sets == nullptr || count == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t set, uint32_t binding) {
    bindings[n] = binding;
    sets[n] = set;
    ++n;
  };
  push(0, 0);
  push(0, 1);
  push(0, 2);
  if (skinned) {
    push(0, 3);
  }
  push(1, 0);
  push(1, 1);
  *count = n;
  ASSERT(n == (skinned ? k_skinned_pbr_descriptor_binding_count
                       : k_pbr_descriptor_binding_count));
}

}  // namespace Blunder
