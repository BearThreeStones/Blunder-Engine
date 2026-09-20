#pragma once

#include <cstdint>

namespace Blunder {

enum class ShaderDescriptorKind : uint8_t {
  UniformBuffer = 0,
  SampledImage = 1,
  Sampler = 2,
  StorageBuffer = 3,
  StorageImage = 4,
};

constexpr uint32_t k_shader_stage_vertex = 1u;
constexpr uint32_t k_shader_stage_fragment = 2u;
constexpr uint32_t k_shader_stage_compute = 4u;
constexpr uint32_t k_shader_stage_task = 8u;
constexpr uint32_t k_shader_stage_mesh = 16u;

struct ShaderResourceBinding {
  uint32_t set{0};
  uint32_t binding{0};
  ShaderDescriptorKind kind{ShaderDescriptorKind::UniformBuffer};
  uint32_t stage_mask{k_shader_stage_vertex | k_shader_stage_fragment};
};

constexpr uint32_t k_max_expected_descriptor_bindings = 24;

struct ShaderResourceLayout {
  ShaderResourceBinding bindings[k_max_expected_descriptor_bindings]{};
  uint32_t count{0};
};

constexpr uint32_t k_pbr_descriptor_binding_count = 9;
constexpr uint32_t k_skinned_pbr_descriptor_binding_count = 10;
constexpr uint32_t k_shadow_descriptor_binding_count = 1;
constexpr uint32_t k_skinned_shadow_descriptor_binding_count = 2;
/// Deferred G-buffer geometry: set 0 mesh UBO (+ bone palette when skinned),
/// set 1 Bindless textures + samplers.
constexpr uint32_t k_gbuffer_descriptor_binding_count = 3;
constexpr uint32_t k_skinned_gbuffer_descriptor_binding_count = 4;
/// Deferred lighting: UBO, 3 G-buffer planes, depth, shadow map, shadow sampler,
/// receiver light-mask SSBO, clustered lights, froxel indices/counts, clustered
/// receiver-mask SSBO.
constexpr uint32_t k_deferred_lighting_descriptor_binding_count = 16;
constexpr uint32_t k_froxel_fill_descriptor_binding_count = 5;
constexpr uint32_t k_vrs_sobel_descriptor_binding_count = 3;
constexpr uint32_t k_vrs_rate_mask_descriptor_binding_count = 2;
constexpr uint32_t k_gpu_driven_pbr_descriptor_binding_count = 10;
constexpr uint32_t k_gpu_driven_gbuffer_descriptor_binding_count = 4;
constexpr uint32_t k_gpu_driven_shadow_descriptor_binding_count = 2;
constexpr uint32_t k_meshlet_cull_descriptor_binding_count = 10;
constexpr uint32_t k_hiz_pyramid_descriptor_binding_count = 4;
constexpr uint32_t k_gpu_driven_mesh_descriptor_binding_count = 15;

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

/// `engine/shaders/gbuffer.slang` / `gbuffer_skinned.slang` record-path bindings.
void fillGBufferExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                 uint32_t* count, bool skinned,
                                 ShaderDescriptorKind* kinds);

/// `engine/shaders/deferred_lighting.slang` record-path bindings (set 0 only).
void fillDeferredLightingExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                          uint32_t* count,
                                          ShaderDescriptorKind* kinds);

void fillFroxelFillExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                    uint32_t* count,
                                    ShaderDescriptorKind* kinds);

void fillVrsSobelExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                  uint32_t* count,
                                  ShaderDescriptorKind* kinds);

void fillVrsRateMaskExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                     uint32_t* count,
                                     ShaderDescriptorKind* kinds);

void fillGpuDrivenPbrExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                      uint32_t* count,
                                      ShaderDescriptorKind* kinds);

void fillGpuDrivenGBufferExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                          uint32_t* count,
                                          ShaderDescriptorKind* kinds);

void fillGpuDrivenShadowExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                         uint32_t* count,
                                         ShaderDescriptorKind* kinds);

void fillMeshletCullExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                     uint32_t* count,
                                     ShaderDescriptorKind* kinds);

void fillHizPyramidExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                    uint32_t* count,
                                    ShaderDescriptorKind* kinds);

void fillGpuDrivenMeshExpectedBindings(uint32_t* bindings, uint32_t* sets,
                                       uint32_t* count,
                                       ShaderDescriptorKind* kinds);

}  // namespace Blunder
