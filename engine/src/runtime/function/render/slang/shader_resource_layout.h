#pragma once

#include <cstdint>

namespace Blunder {

enum class ShaderDescriptorKind : uint8_t {
  UniformBuffer = 0,
  SampledImage = 1,
  Sampler = 2,
};

constexpr uint32_t k_shader_stage_vertex = 1u;
constexpr uint32_t k_shader_stage_fragment = 2u;
constexpr uint32_t k_shader_stage_compute = 4u;

struct ShaderResourceBinding {
  uint32_t set{0};
  uint32_t binding{0};
  ShaderDescriptorKind kind{ShaderDescriptorKind::UniformBuffer};
  uint32_t stage_mask{k_shader_stage_vertex | k_shader_stage_fragment};
};

constexpr uint32_t k_max_expected_descriptor_bindings = 16;

struct ShaderResourceLayout {
  ShaderResourceBinding bindings[k_max_expected_descriptor_bindings]{};
  uint32_t count{0};
};

constexpr uint32_t k_pbr_descriptor_binding_count = 5;
constexpr uint32_t k_skinned_pbr_descriptor_binding_count = 6;
constexpr uint32_t k_shadow_descriptor_binding_count = 1;
constexpr uint32_t k_skinned_shadow_descriptor_binding_count = 2;

/// True when extracted (set, binding[, kind]) tuples equal expected tuples.
/// `expected_sets` nullptr means every expected binding is set 0.
/// `expected_kinds` nullptr skips kind comparison.
bool shaderResourceBindingsMatch(const ShaderResourceLayout& layout,
                                 const uint32_t* expected_bindings,
                                 uint32_t expected_count,
                                 const uint32_t* expected_sets = nullptr,
                                 const ShaderDescriptorKind* expected_kinds =
                                     nullptr);

void fillSequentialExpectedBindings(uint32_t* bindings, uint32_t* count,
                                    uint32_t n);

void fillPbrMeshExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                 uint32_t* count, bool skinned,
                                 ShaderDescriptorKind* kinds);

}  // namespace Blunder
