#include "runtime/function/render/slang/shader_resource_layout.h"

#include "EASTL/algorithm.h"
#include "EASTL/sort.h"

#include "runtime/core/base/macro.h"

namespace Blunder {

namespace {

struct BindingKey {
  uint32_t set{0};
  uint32_t binding{0};
  uint32_t kind{0};

  bool operator<(const BindingKey& other) const {
    if (set != other.set) {
      return set < other.set;
    }
    if (binding != other.binding) {
      return binding < other.binding;
    }
    return kind < other.kind;
  }

  bool operator==(const BindingKey& other) const {
    return set == other.set && binding == other.binding && kind == other.kind;
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
                                 const uint32_t* expected_sets,
                                 const ShaderDescriptorKind* expected_kinds) {
  if (layout.count > k_max_expected_descriptor_bindings) {
    return false;
  }
  if (expected_count > k_max_expected_descriptor_bindings) {
    return false;
  }

  const bool compare_kinds = expected_kinds != nullptr;
  BindingKey extracted[k_max_expected_descriptor_bindings];
  for (uint32_t i = 0; i < layout.count; ++i) {
    extracted[i] = {
        layout.bindings[i].set, layout.bindings[i].binding,
        compare_kinds ? static_cast<uint32_t>(layout.bindings[i].kind) : 0};
  }
  const uint32_t extracted_count = uniqueSortedKeys(extracted, layout.count);

  BindingKey expected[k_max_expected_descriptor_bindings];
  uint32_t filled_expected = 0;
  if (expected_bindings != nullptr) {
    for (uint32_t i = 0; i < expected_count; ++i) {
      expected[i] = {
          expected_sets != nullptr ? expected_sets[i] : 0, expected_bindings[i],
          compare_kinds ? static_cast<uint32_t>(expected_kinds[i]) : 0};
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
                                 uint32_t* count, bool skinned,
                                 ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t set, uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = set;
    kinds[n] = kind;
    ++n;
  };
  push(0, 0, ShaderDescriptorKind::UniformBuffer);
  push(0, 1, ShaderDescriptorKind::SampledImage);
  push(0, 2, ShaderDescriptorKind::Sampler);
  if (skinned) {
    push(0, 3, ShaderDescriptorKind::UniformBuffer);
    push(0, 4, ShaderDescriptorKind::SampledImage);
    push(0, 5, ShaderDescriptorKind::StorageBuffer);
    push(0, 6, ShaderDescriptorKind::SampledImage);
    push(0, 7, ShaderDescriptorKind::SampledImage);
  } else {
    push(0, 3, ShaderDescriptorKind::SampledImage);
    push(0, 4, ShaderDescriptorKind::StorageBuffer);
    push(0, 5, ShaderDescriptorKind::SampledImage);
    push(0, 6, ShaderDescriptorKind::SampledImage);
  }
  push(1, 0, ShaderDescriptorKind::SampledImage);
  push(1, 1, ShaderDescriptorKind::Sampler);
  *count = n;
  ASSERT(n == (skinned ? k_skinned_pbr_descriptor_binding_count
                       : k_pbr_descriptor_binding_count));
}

void fillGBufferExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                 uint32_t* count, bool skinned,
                                 ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t set, uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = set;
    kinds[n] = kind;
    ++n;
  };
  push(0, 0, ShaderDescriptorKind::UniformBuffer);
  if (skinned) {
    push(0, 1, ShaderDescriptorKind::UniformBuffer);
  }
  push(1, 0, ShaderDescriptorKind::SampledImage);
  push(1, 1, ShaderDescriptorKind::Sampler);
  *count = n;
  ASSERT(n == (skinned ? k_skinned_gbuffer_descriptor_binding_count
                       : k_gbuffer_descriptor_binding_count));
}

void fillDeferredLightingExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                          uint32_t* count,
                                          ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = 0;
    kinds[n] = kind;
    ++n;
  };
  push(0, ShaderDescriptorKind::UniformBuffer);  // lighting UBO
  push(1, ShaderDescriptorKind::SampledImage);   // albedo + AO
  push(2, ShaderDescriptorKind::SampledImage);   // oct normal + metal + rough
  push(3, ShaderDescriptorKind::SampledImage);   // receiver id
  push(4, ShaderDescriptorKind::SampledImage);   // offscreen depth
  push(5, ShaderDescriptorKind::SampledImage);   // directional shadow map
  push(6, ShaderDescriptorKind::Sampler);        // shadow comparison sampler
  push(7, ShaderDescriptorKind::StorageBuffer);  // receiver light masks
  push(8, ShaderDescriptorKind::StorageBuffer);  // clustered point/spot lights
  push(9, ShaderDescriptorKind::StorageBuffer);  // froxel light indices
  push(10, ShaderDescriptorKind::StorageBuffer); // froxel light counts
  push(11, ShaderDescriptorKind::StorageBuffer); // clustered receiver masks
  push(12, ShaderDescriptorKind::SampledImage);   // VSM physical pages
  push(13, ShaderDescriptorKind::StorageBuffer);  // VSM page table
  push(14, ShaderDescriptorKind::SampledImage);   // point cube array
  push(15, ShaderDescriptorKind::SampledImage);   // spot 2D array
  *count = n;
  ASSERT(n == k_deferred_lighting_descriptor_binding_count);
}

void fillFroxelFillExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                    uint32_t* count,
                                    ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = 0;
    kinds[n] = kind;
    ++n;
  };
  push(0, ShaderDescriptorKind::UniformBuffer);  // fill UBO
  push(1, ShaderDescriptorKind::StorageBuffer);  // clustered lights
  push(2, ShaderDescriptorKind::StorageBuffer);  // froxel indices
  push(3, ShaderDescriptorKind::StorageBuffer);  // froxel counts
  push(4, ShaderDescriptorKind::StorageBuffer);  // overflow counter
  *count = n;
  ASSERT(n == k_froxel_fill_descriptor_binding_count);
}

void fillVrsSobelExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                  uint32_t* count,
                                  ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = 0;
    kinds[n] = kind;
    ++n;
  };
  push(0, ShaderDescriptorKind::UniformBuffer);
  push(1, ShaderDescriptorKind::SampledImage);
  push(2, ShaderDescriptorKind::StorageImage);
  *count = n;
  ASSERT(n == k_vrs_sobel_descriptor_binding_count);
}

void fillVrsRateMaskExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                     uint32_t* count,
                                     ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = 0;
    kinds[n] = kind;
    ++n;
  };
  push(0, ShaderDescriptorKind::UniformBuffer);
  push(1, ShaderDescriptorKind::SampledImage);
  *count = n;
  ASSERT(n == k_vrs_rate_mask_descriptor_binding_count);
}

void fillGpuDrivenPbrExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                      uint32_t* count,
                                      ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t set, uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = set;
    kinds[n] = kind;
    ++n;
  };
  push(0, 0, ShaderDescriptorKind::UniformBuffer);
  push(0, 1, ShaderDescriptorKind::SampledImage);
  push(0, 2, ShaderDescriptorKind::Sampler);
  push(0, 3, ShaderDescriptorKind::StorageBuffer);
  push(0, 4, ShaderDescriptorKind::SampledImage);
  push(0, 5, ShaderDescriptorKind::StorageBuffer);
  push(0, 6, ShaderDescriptorKind::SampledImage);
  push(0, 7, ShaderDescriptorKind::SampledImage);
  push(0, 8, ShaderDescriptorKind::StorageBuffer);
  push(1, 0, ShaderDescriptorKind::SampledImage);
  push(1, 1, ShaderDescriptorKind::Sampler);
  *count = n;
  ASSERT(n == k_gpu_driven_pbr_descriptor_binding_count);
}

void fillGpuDrivenGBufferExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                          uint32_t* count,
                                          ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t set, uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = set;
    kinds[n] = kind;
    ++n;
  };
  push(0, 0, ShaderDescriptorKind::UniformBuffer);
  push(0, 1, ShaderDescriptorKind::StorageBuffer);
  push(0, 2, ShaderDescriptorKind::StorageBuffer);
  push(1, 0, ShaderDescriptorKind::SampledImage);
  push(1, 1, ShaderDescriptorKind::Sampler);
  *count = n;
  ASSERT(n == k_gpu_driven_gbuffer_descriptor_binding_count);
}

void fillGpuDrivenShadowExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                         uint32_t* count,
                                         ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t set, uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = set;
    kinds[n] = kind;
    ++n;
  };
  push(0, 0, ShaderDescriptorKind::UniformBuffer);
  push(0, 1, ShaderDescriptorKind::StorageBuffer);
  push(0, 2, ShaderDescriptorKind::StorageBuffer);
  *count = n;
  ASSERT(n == k_gpu_driven_shadow_descriptor_binding_count);
}

void fillMeshletCullExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                     uint32_t* count,
                                     ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = 0;
    kinds[n] = kind;
    ++n;
  };
  push(0, ShaderDescriptorKind::UniformBuffer);
  push(1, ShaderDescriptorKind::StorageBuffer);
  push(2, ShaderDescriptorKind::StorageBuffer);
  push(3, ShaderDescriptorKind::SampledImage);
  push(4, ShaderDescriptorKind::Sampler);
  push(5, ShaderDescriptorKind::StorageBuffer);
  push(6, ShaderDescriptorKind::StorageBuffer);
  push(7, ShaderDescriptorKind::StorageBuffer);
  push(8, ShaderDescriptorKind::StorageBuffer);
  push(9, ShaderDescriptorKind::StorageBuffer);
  push(10, ShaderDescriptorKind::StorageBuffer);
  push(11, ShaderDescriptorKind::StorageBuffer);
  push(12, ShaderDescriptorKind::StorageBuffer);
  *count = n;
  ASSERT(n == k_meshlet_cull_descriptor_binding_count);
}

void fillMeshletEmitExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                     uint32_t* count,
                                     ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = 0;
    kinds[n] = kind;
    ++n;
  };
  push(0, ShaderDescriptorKind::UniformBuffer);
  for (uint32_t b = 1; b <= 12; ++b) {
    push(b, ShaderDescriptorKind::StorageBuffer);
  }
  *count = n;
  ASSERT(n == k_meshlet_emit_descriptor_binding_count);
}

void fillHizPyramidExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                    uint32_t* count,
                                    ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = 0;
    kinds[n] = kind;
    ++n;
  };
  push(0, ShaderDescriptorKind::UniformBuffer);
  push(1, ShaderDescriptorKind::SampledImage);
  push(2, ShaderDescriptorKind::Sampler);
  push(3, ShaderDescriptorKind::StorageImage);
  *count = n;
  ASSERT(n == k_hiz_pyramid_descriptor_binding_count);
}

void fillGpuDrivenMeshExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                       uint32_t* count,
                                       ShaderDescriptorKind* kinds) {
  if (bindings == nullptr || sets == nullptr || count == nullptr ||
      kinds == nullptr) {
    return;
  }
  uint32_t n = 0;
  auto push = [&](uint32_t set, uint32_t binding, ShaderDescriptorKind kind) {
    bindings[n] = binding;
    sets[n] = set;
    kinds[n] = kind;
    ++n;
  };
  push(0, 0, ShaderDescriptorKind::UniformBuffer);
  push(0, 1, ShaderDescriptorKind::SampledImage);
  push(0, 2, ShaderDescriptorKind::Sampler);
  push(0, 3, ShaderDescriptorKind::StorageBuffer);
  push(0, 4, ShaderDescriptorKind::StorageBuffer);
  push(0, 5, ShaderDescriptorKind::StorageBuffer);
  push(0, 6, ShaderDescriptorKind::StorageBuffer);
  push(0, 7, ShaderDescriptorKind::StorageBuffer);
  push(0, 8, ShaderDescriptorKind::StorageBuffer);
  push(0, 9, ShaderDescriptorKind::SampledImage);
  push(0, 10, ShaderDescriptorKind::StorageBuffer);
  push(0, 11, ShaderDescriptorKind::SampledImage);
  push(0, 12, ShaderDescriptorKind::SampledImage);
  push(0, 13, ShaderDescriptorKind::StorageBuffer);
  push(1, 0, ShaderDescriptorKind::SampledImage);
  push(1, 1, ShaderDescriptorKind::Sampler);
  *count = n;
  ASSERT(n == k_gpu_driven_mesh_descriptor_binding_count);
}

}  // namespace Blunder
