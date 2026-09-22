#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/slang/engine_gpu_cache.h"
#include "runtime/function/render/slang/shader_resource_layout.h"
#include "runtime/function/render/slang/slang_compiler.h"

#include "EASTL/shared_ptr.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

void appendBinding(Blunder::ShaderResourceLayout& layout,
                   Blunder::ShaderResourceBinding binding) {
  layout.bindings[layout.count++] = binding;
}

bool layoutHasSequentialBindings(const Blunder::ShaderResourceLayout& layout,
                                 uint32_t count) {
  uint32_t expected[Blunder::k_max_expected_descriptor_bindings];
  Blunder::ShaderDescriptorKind
      kinds[Blunder::k_max_expected_descriptor_bindings]{};
  uint32_t expected_count = 0;
  Blunder::fillSequentialExpectedBindings(expected, &expected_count, count);
  for (uint32_t i = 0; i < expected_count; ++i) {
    kinds[i] = Blunder::ShaderDescriptorKind::UniformBuffer;
  }
  return Blunder::shaderResourceBindingsMatch(layout, expected, expected_count,
                                              nullptr, kinds);
}

void dumpBindings(const char* label,
                  const Blunder::ShaderResourceLayout& layout) {
  const uint32_t n = layout.count > Blunder::k_max_expected_descriptor_bindings
                         ? Blunder::k_max_expected_descriptor_bindings
                         : layout.count;
  std::fprintf(stderr, "%s (count=%u):", label, layout.count);
  for (uint32_t i = 0; i < n; ++i) {
    std::fprintf(stderr, " %u:%u/%u", layout.bindings[i].set,
                 layout.bindings[i].binding,
                 static_cast<unsigned>(layout.bindings[i].kind));
  }
  std::fprintf(stderr, "\n");
}

bool layoutMatchesPbr(const Blunder::ShaderResourceLayout& layout, bool skinned) {
  uint32_t expected[Blunder::k_max_expected_descriptor_bindings];
  uint32_t sets[Blunder::k_max_expected_descriptor_bindings];
  Blunder::ShaderDescriptorKind
      kinds[Blunder::k_max_expected_descriptor_bindings]{};
  uint32_t expected_count = 0;
  Blunder::fillPbrMeshExpectedBindings(expected, sets, &expected_count, skinned,
                                       kinds);
  return Blunder::shaderResourceBindingsMatch(layout, expected, expected_count,
                                              sets, kinds);
}

bool layoutMatchesGBuffer(const Blunder::ShaderResourceLayout& layout,
                          bool skinned) {
  uint32_t expected[Blunder::k_max_expected_descriptor_bindings];
  uint32_t sets[Blunder::k_max_expected_descriptor_bindings];
  Blunder::ShaderDescriptorKind
      kinds[Blunder::k_max_expected_descriptor_bindings]{};
  uint32_t expected_count = 0;
  Blunder::fillGBufferExpectedBindings(expected, sets, &expected_count, skinned,
                                       kinds);
  return Blunder::shaderResourceBindingsMatch(layout, expected, expected_count,
                                              sets, kinds);
}

bool layoutMatchesDeferredLighting(
    const Blunder::ShaderResourceLayout& layout) {
  uint32_t expected[Blunder::k_max_expected_descriptor_bindings];
  uint32_t sets[Blunder::k_max_expected_descriptor_bindings];
  Blunder::ShaderDescriptorKind
      kinds[Blunder::k_max_expected_descriptor_bindings]{};
  uint32_t expected_count = 0;
  Blunder::fillDeferredLightingExpectedBindings(expected, sets, &expected_count,
                                                kinds);
  return Blunder::shaderResourceBindingsMatch(layout, expected, expected_count,
                                              sets, kinds);
}

bool layoutUsesSet(const Blunder::ShaderResourceLayout& layout, uint32_t set) {
  for (uint32_t i = 0; i < layout.count; ++i) {
    if (layout.bindings[i].set == set) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  std::filesystem::path cache_root = std::filesystem::temp_directory_path() /
                                     "blunder-layout-test-gpu-cache";
#ifdef _WIN32
  cache_root += std::to_string(_getpid());
  _putenv_s(k_gpu_cache_dir_env, cache_root.string().c_str());
#else
  cache_root += std::to_string(getpid());
  setenv(k_gpu_cache_dir_env, cache_root.string().c_str(), 1);
#endif
  std::error_code ec;
  std::filesystem::create_directories(cache_root, ec);

  {
    uint32_t expected[k_max_expected_descriptor_bindings];
    uint32_t expected_count = 0;
    fillSequentialExpectedBindings(expected, &expected_count, 2);
    ShaderResourceLayout layout;
    appendBinding(layout, {0, 1, ShaderDescriptorKind::Sampler,
                           k_shader_stage_fragment});
    appendBinding(layout, {0, 0, ShaderDescriptorKind::UniformBuffer,
                           k_shader_stage_vertex});
    expect_true("compare helper equal after sort",
                shaderResourceBindingsMatch(layout, expected, expected_count));
    uint32_t wrong[] = {0};
    expect_true("compare helper fails on missing binding",
                !shaderResourceBindingsMatch(layout, wrong, 1));
    uint32_t extra[] = {0, 1, 2};
    expect_true("compare helper fails on extra expected binding",
                !shaderResourceBindingsMatch(layout, extra, 3));
    uint32_t same_count_wrong[] = {0, 2};
    expect_true("compare helper fails on same count different members",
                !shaderResourceBindingsMatch(layout, same_count_wrong, 2));

    ShaderResourceLayout mixed_sets;
    appendBinding(mixed_sets, {0, 0, ShaderDescriptorKind::UniformBuffer,
                               k_shader_stage_vertex});
    appendBinding(mixed_sets, {1, 0, ShaderDescriptorKind::SampledImage,
                               k_shader_stage_fragment});
    uint32_t mixed_bindings[] = {0, 0};
    uint32_t mixed_sets_ok[] = {0, 1};
    uint32_t mixed_sets_bad[] = {0, 0};
    expect_true("compare helper honors expected_sets",
                shaderResourceBindingsMatch(mixed_sets, mixed_bindings, 2,
                                            mixed_sets_ok));
    expect_true("compare helper fails when expected_sets disagree",
                !shaderResourceBindingsMatch(mixed_sets, mixed_bindings, 2,
                                             mixed_sets_bad));
    expect_true("compare helper nullptr expected_sets means all set 0",
                !shaderResourceBindingsMatch(mixed_sets, mixed_bindings, 2,
                                             nullptr));
    ShaderDescriptorKind mixed_kinds_ok[] = {
        ShaderDescriptorKind::UniformBuffer, ShaderDescriptorKind::SampledImage};
    ShaderDescriptorKind mixed_kinds_wrong[] = {
        ShaderDescriptorKind::UniformBuffer, ShaderDescriptorKind::Sampler};
    expect_true("compare helper honors expected_kinds",
                shaderResourceBindingsMatch(mixed_sets, mixed_bindings, 2,
                                            mixed_sets_ok, mixed_kinds_ok));
    expect_true("compare helper fails when expected_kinds disagree",
                !shaderResourceBindingsMatch(mixed_sets, mixed_bindings, 2,
                                             mixed_sets_ok, mixed_kinds_wrong));
    expect_true("compare helper nullptr expected_kinds skips kind",
                shaderResourceBindingsMatch(mixed_sets, mixed_bindings, 2,
                                            mixed_sets_ok, nullptr));
  }

  {
    uint32_t bindings[k_max_expected_descriptor_bindings];
    uint32_t sets[k_max_expected_descriptor_bindings];
    ShaderDescriptorKind kinds[k_max_expected_descriptor_bindings]{};
    uint32_t count = 0;
    fillPbrMeshExpectedBindings(bindings, sets, &count, false, kinds);
    expect_true("unskinned pbr expected count",
                count == k_pbr_descriptor_binding_count);
    expect_true("unskinned pbr set0 ubo", sets[0] == 0 && bindings[0] == 0 &&
                kinds[0] == ShaderDescriptorKind::UniformBuffer);
    expect_true("unskinned pbr set0 shadow image",
                sets[1] == 0 && bindings[1] == 1 &&
                kinds[1] == ShaderDescriptorKind::SampledImage);
    expect_true("unskinned pbr set0 shadow sampler",
                sets[2] == 0 && bindings[2] == 2 &&
                kinds[2] == ShaderDescriptorKind::Sampler);
    expect_true("unskinned pbr vsm pages",
                sets[3] == 0 && bindings[3] == 3 &&
                kinds[3] == ShaderDescriptorKind::SampledImage);
    expect_true("unskinned pbr set1 textures", sets[7] == 1 && bindings[7] == 0 &&
                kinds[7] == ShaderDescriptorKind::SampledImage);
    expect_true("unskinned pbr set1 samplers", sets[8] == 1 && bindings[8] == 1 &&
                kinds[8] == ShaderDescriptorKind::Sampler);

    fillPbrMeshExpectedBindings(bindings, sets, &count, true, kinds);
    expect_true("skinned pbr expected count",
                count == k_skinned_pbr_descriptor_binding_count);
    expect_true("skinned pbr bone ubo", sets[3] == 0 && bindings[3] == 3 &&
                kinds[3] == ShaderDescriptorKind::UniformBuffer);
    expect_true("skinned pbr set1 textures", sets[8] == 1 && bindings[8] == 0 &&
                kinds[8] == ShaderDescriptorKind::SampledImage);
    expect_true("skinned pbr set1 samplers", sets[9] == 1 && bindings[9] == 1 &&
                kinds[9] == ShaderDescriptorKind::Sampler);
  }

  {
    uint32_t bindings[k_max_expected_descriptor_bindings];
    uint32_t sets[k_max_expected_descriptor_bindings];
    ShaderDescriptorKind kinds[k_max_expected_descriptor_bindings]{};
    uint32_t count = 0;
    fillGBufferExpectedBindings(bindings, sets, &count, false, kinds);
    expect_true("gbuffer expected count",
                count == k_gbuffer_descriptor_binding_count);
    expect_true("gbuffer set0 ubo", sets[0] == 0 && bindings[0] == 0 &&
                kinds[0] == ShaderDescriptorKind::UniformBuffer);
    expect_true("gbuffer set1 textures", sets[1] == 1 && bindings[1] == 0 &&
                kinds[1] == ShaderDescriptorKind::SampledImage);
    expect_true("gbuffer set1 samplers", sets[2] == 1 && bindings[2] == 1 &&
                kinds[2] == ShaderDescriptorKind::Sampler);

    fillGBufferExpectedBindings(bindings, sets, &count, true, kinds);
    expect_true("skinned gbuffer expected count",
                count == k_skinned_gbuffer_descriptor_binding_count);
    expect_true("skinned gbuffer bone ubo", sets[1] == 0 && bindings[1] == 1 &&
                kinds[1] == ShaderDescriptorKind::UniformBuffer);
    expect_true("skinned gbuffer set1 textures",
                sets[2] == 1 && bindings[2] == 0 &&
                kinds[2] == ShaderDescriptorKind::SampledImage);

    fillDeferredLightingExpectedBindings(bindings, sets, &count, kinds);
    expect_true("deferred lighting expected count",
                count == k_deferred_lighting_descriptor_binding_count);
    bool lighting_all_set0 = true;
    for (uint32_t i = 0; i < count; ++i) {
      lighting_all_set0 = lighting_all_set0 && sets[i] == 0 && bindings[i] == i;
    }
    expect_true("deferred lighting sequential set 0 bindings",
                lighting_all_set0);
    expect_true("deferred lighting ubo",
                kinds[0] == ShaderDescriptorKind::UniformBuffer);
    expect_true("deferred lighting gbuffer + depth + shadow images",
                kinds[1] == ShaderDescriptorKind::SampledImage &&
                    kinds[2] == ShaderDescriptorKind::SampledImage &&
                    kinds[3] == ShaderDescriptorKind::SampledImage &&
                    kinds[4] == ShaderDescriptorKind::SampledImage &&
                    kinds[5] == ShaderDescriptorKind::SampledImage);
    expect_true("deferred lighting shadow sampler",
                kinds[6] == ShaderDescriptorKind::Sampler);
    expect_true("deferred lighting receiver mask ssbo",
                kinds[7] == ShaderDescriptorKind::StorageBuffer);
    expect_true("deferred lighting clustered lights ssbo",
                kinds[8] == ShaderDescriptorKind::StorageBuffer);
    expect_true("deferred lighting froxel indices ssbo",
                kinds[9] == ShaderDescriptorKind::StorageBuffer);
    expect_true("deferred lighting froxel counts ssbo",
                kinds[10] == ShaderDescriptorKind::StorageBuffer);
    expect_true("deferred lighting clustered masks ssbo",
                kinds[11] == ShaderDescriptorKind::StorageBuffer);
    expect_true("deferred lighting vsm pages",
                kinds[12] == ShaderDescriptorKind::SampledImage);
    expect_true("deferred lighting page table",
                kinds[13] == ShaderDescriptorKind::StorageBuffer);
    expect_true("deferred lighting point cubes",
                kinds[14] == ShaderDescriptorKind::SampledImage);
    expect_true("deferred lighting spot maps",
                kinds[15] == ShaderDescriptorKind::SampledImage);

    fillFroxelFillExpectedBindings(bindings, sets, &count, kinds);
    expect_true("froxel fill expected count",
                count == k_froxel_fill_descriptor_binding_count);
    expect_true("froxel fill ubo",
                kinds[0] == ShaderDescriptorKind::UniformBuffer);
    expect_true("froxel fill storage buffers",
                kinds[1] == ShaderDescriptorKind::StorageBuffer &&
                    kinds[2] == ShaderDescriptorKind::StorageBuffer &&
                    kinds[3] == ShaderDescriptorKind::StorageBuffer &&
                    kinds[4] == ShaderDescriptorKind::StorageBuffer);
  }

  SlangCompiler compiler;
  compiler.initialize();

  {
    const SlangCompiler::GraphicsProgramResult pbr =
        compiler.compileGraphicsProgram("engine/shaders/pbr.slang");
    if (!layoutMatchesPbr(pbr.layout, false)) {
      dumpBindings("pbr.slang", pbr.layout);
    }
    expect_true("pbr.slang compact set 0 plus bindless set 1",
                layoutMatchesPbr(pbr.layout, false));
    uint32_t wrong[] = {0};
    expect_true("pbr.slang mismatch vs binding 0 only",
                !shaderResourceBindingsMatch(pbr.layout, wrong, 1));
  }

  {
    const SlangCompiler::GraphicsProgramResult skinned =
        compiler.compileGraphicsProgram("engine/shaders/pbr_skinned.slang");
    if (!layoutMatchesPbr(skinned.layout, true)) {
      dumpBindings("pbr_skinned.slang", skinned.layout);
    }
    expect_true("pbr_skinned.slang compact set 0 plus bindless set 1",
                layoutMatchesPbr(skinned.layout, true));
  }

  {
    const SlangCompiler::GraphicsProgramResult shadow =
        compiler.compileGraphicsProgram("engine/shaders/shadow_depth.slang");
    if (!layoutHasSequentialBindings(shadow.layout,
                                     k_shadow_descriptor_binding_count)) {
      dumpBindings("shadow_depth.slang", shadow.layout);
    }
    expect_true("shadow_depth.slang binding 0",
                layoutHasSequentialBindings(shadow.layout,
                                            k_shadow_descriptor_binding_count));
  }

  {
    const SlangCompiler::GraphicsProgramResult skinned_shadow =
        compiler.compileGraphicsProgram(
            "engine/shaders/shadow_depth_skinned.slang");
    if (!layoutHasSequentialBindings(
            skinned_shadow.layout, k_skinned_shadow_descriptor_binding_count)) {
      dumpBindings("shadow_depth_skinned.slang", skinned_shadow.layout);
    }
    expect_true("shadow_depth_skinned.slang bindings 0-1",
                layoutHasSequentialBindings(
                    skinned_shadow.layout,
                    k_skinned_shadow_descriptor_binding_count));
  }

  {
    const SlangCompiler::GraphicsProgramResult grid =
        compiler.compileGraphicsProgram("engine/shaders/grid.slang");
    if (!layoutHasSequentialBindings(grid.layout, 1)) {
      dumpBindings("grid.slang", grid.layout);
    }
    expect_true("grid.slang binding 0",
                layoutHasSequentialBindings(grid.layout, 1));
  }

  {
    const SlangCompiler::GraphicsProgramResult camera_gizmo =
        compiler.compileGraphicsProgram("engine/shaders/camera_gizmo.slang");
    if (!layoutHasSequentialBindings(camera_gizmo.layout, 1)) {
      dumpBindings("camera_gizmo.slang", camera_gizmo.layout);
    }
    expect_true("camera_gizmo.slang compiles graphics VS/FS",
                camera_gizmo.vertex.spirv_code.size() > 0 &&
                    camera_gizmo.fragment.spirv_code.size() > 0);
    expect_true("camera_gizmo.slang binding 0",
                layoutHasSequentialBindings(camera_gizmo.layout, 1));
  }

  // Deferred Render Path (ADR 0062): G-buffer geometry binds the Bindless
  // table like Forward opaque; lighting stays off the table (set 0 only).
  {
    const SlangCompiler::GraphicsProgramResult gbuffer =
        compiler.compileGraphicsProgram("engine/shaders/gbuffer.slang");
    if (!layoutMatchesGBuffer(gbuffer.layout, false)) {
      dumpBindings("gbuffer.slang", gbuffer.layout);
    }
    expect_true("gbuffer.slang set 0 ubo plus bindless set 1",
                layoutMatchesGBuffer(gbuffer.layout, false));
    expect_true("gbuffer.slang does not match pbr layout",
                !layoutMatchesPbr(gbuffer.layout, false));
  }

  {
    const SlangCompiler::GraphicsProgramResult skinned_gbuffer =
        compiler.compileGraphicsProgram("engine/shaders/gbuffer_skinned.slang");
    if (!layoutMatchesGBuffer(skinned_gbuffer.layout, true)) {
      dumpBindings("gbuffer_skinned.slang", skinned_gbuffer.layout);
    }
    expect_true("gbuffer_skinned.slang set 0 ubo + palette plus bindless set 1",
                layoutMatchesGBuffer(skinned_gbuffer.layout, true));
    expect_true("gbuffer_skinned.slang does not match static gbuffer layout",
                !layoutMatchesGBuffer(skinned_gbuffer.layout, false));
  }

  {
    const SlangCompiler::GraphicsProgramResult lighting =
        compiler.compileGraphicsProgram("engine/shaders/deferred_lighting.slang");
    if (!layoutMatchesDeferredLighting(lighting.layout)) {
      dumpBindings("deferred_lighting.slang", lighting.layout);
    }
    expect_true("deferred_lighting.slang bindings 0-15 set 0",
                layoutMatchesDeferredLighting(lighting.layout));
    expect_true("deferred_lighting.slang does not bind the Bindless set 1",
                !layoutUsesSet(lighting.layout, 1));
  }

  {
    uint32_t bindings[k_max_expected_descriptor_bindings];
    uint32_t sets[k_max_expected_descriptor_bindings];
    ShaderDescriptorKind kinds[k_max_expected_descriptor_bindings]{};
    uint32_t count = 0;
    fillFroxelFillExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::ComputeProgramResult fill =
        compiler.compileComputeProgram("engine/shaders/froxel_fill.slang");
    if (!shaderResourceBindingsMatch(fill.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("froxel_fill.slang", fill.layout);
    }
    expect_true("froxel_fill.slang layout",
                shaderResourceBindingsMatch(fill.layout, bindings, count, sets,
                                            kinds));
    expect_true("froxel_fill.slang does not bind the Bindless set 1",
                !layoutUsesSet(fill.layout, 1));
  }

  {
    uint32_t bindings[k_max_expected_descriptor_bindings];
    uint32_t sets[k_max_expected_descriptor_bindings];
    ShaderDescriptorKind kinds[k_max_expected_descriptor_bindings]{};
    uint32_t count = 0;
    fillVrsSobelExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::ComputeProgramResult sobel =
        compiler.compileComputeProgram("engine/shaders/vrs_sobel.slang");
    if (!shaderResourceBindingsMatch(sobel.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("vrs_sobel.slang", sobel.layout);
    }
    expect_true("vrs_sobel.slang layout",
                shaderResourceBindingsMatch(sobel.layout, bindings, count, sets,
                                            kinds));
    expect_true("vrs_sobel.slang does not bind the Bindless set 1",
                !layoutUsesSet(sobel.layout, 1));

    fillVrsRateMaskExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::GraphicsProgramResult mask =
        compiler.compileGraphicsProgram("engine/shaders/vrs_rate_mask.slang");
    if (!shaderResourceBindingsMatch(mask.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("vrs_rate_mask.slang", mask.layout);
    }
    expect_true("vrs_rate_mask.slang layout",
                shaderResourceBindingsMatch(mask.layout, bindings, count, sets,
                                            kinds));
    expect_true("vrs_rate_mask.slang does not bind the Bindless set 1",
                !layoutUsesSet(mask.layout, 1));
  }

  {
    uint32_t bindings[k_max_expected_descriptor_bindings];
    uint32_t sets[k_max_expected_descriptor_bindings];
    ShaderDescriptorKind kinds[k_max_expected_descriptor_bindings]{};
    uint32_t count = 0;
    fillGpuDrivenPbrExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::GraphicsProgramResult gpu_pbr =
        compiler.compileGraphicsProgram("engine/shaders/pbr_gpu_driven.slang");
    if (!shaderResourceBindingsMatch(gpu_pbr.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("pbr_gpu_driven.slang", gpu_pbr.layout);
    }
    expect_true("pbr_gpu_driven.slang layout",
                shaderResourceBindingsMatch(gpu_pbr.layout, bindings, count, sets,
                                            kinds));

    fillGpuDrivenGBufferExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::GraphicsProgramResult gpu_gbuffer =
        compiler.compileGraphicsProgram("engine/shaders/gbuffer_gpu_driven.slang");
    if (!shaderResourceBindingsMatch(gpu_gbuffer.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("gbuffer_gpu_driven.slang", gpu_gbuffer.layout);
    }
    expect_true("gbuffer_gpu_driven.slang layout",
                shaderResourceBindingsMatch(gpu_gbuffer.layout, bindings, count,
                                            sets, kinds));

    fillGpuDrivenShadowExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::GraphicsProgramResult gpu_shadow =
        compiler.compileGraphicsProgram("engine/shaders/shadow_gpu_driven.slang");
    if (!shaderResourceBindingsMatch(gpu_shadow.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("shadow_gpu_driven.slang", gpu_shadow.layout);
    }
    expect_true("shadow_gpu_driven.slang layout",
                shaderResourceBindingsMatch(gpu_shadow.layout, bindings, count,
                                            sets, kinds));

    fillMeshletCullExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::ComputeProgramResult cull =
        compiler.compileComputeProgram("engine/shaders/meshlet_cull.slang");
    if (!shaderResourceBindingsMatch(cull.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("meshlet_cull.slang", cull.layout);
    }
    expect_true("meshlet_cull.slang layout",
                shaderResourceBindingsMatch(cull.layout, bindings, count, sets,
                                            kinds));

    fillMeshletEmitExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::ComputeProgramResult emit =
        compiler.compileComputeProgram("engine/shaders/meshlet_emit.slang");
    if (!shaderResourceBindingsMatch(emit.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("meshlet_emit.slang", emit.layout);
    }
    expect_true("meshlet_emit.slang layout",
                shaderResourceBindingsMatch(emit.layout, bindings, count, sets,
                                            kinds));

    fillHizPyramidExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::ComputeProgramResult hiz =
        compiler.compileComputeProgram("engine/shaders/hiz_pyramid.slang");
    if (!shaderResourceBindingsMatch(hiz.layout, bindings, count, sets, kinds)) {
      dumpBindings("hiz_pyramid.slang", hiz.layout);
    }
    expect_true("hiz_pyramid.slang layout",
                shaderResourceBindingsMatch(hiz.layout, bindings, count, sets,
                                            kinds));

    fillGpuDrivenMeshExpectedBindings(bindings, sets, &count, kinds);
    const SlangCompiler::MeshProgramResult mesh =
        compiler.compileMeshProgram("engine/shaders/pbr_mesh.slang");
    if (!shaderResourceBindingsMatch(mesh.layout, bindings, count, sets,
                                     kinds)) {
      dumpBindings("pbr_mesh.slang", mesh.layout);
    }
    expect_true("pbr_mesh.slang layout",
                shaderResourceBindingsMatch(mesh.layout, bindings, count, sets,
                                            kinds));
  }

  {
    uint32_t bindings[] = {0, 1, 2, 3, 4, 5};
    uint32_t sets[] = {0, 0, 0, 0, 0, 0};
    ShaderDescriptorKind kinds[] = {
        ShaderDescriptorKind::UniformBuffer, ShaderDescriptorKind::StorageBuffer,
        ShaderDescriptorKind::StorageBuffer, ShaderDescriptorKind::StorageBuffer,
        ShaderDescriptorKind::StorageBuffer, ShaderDescriptorKind::StorageBuffer};
    const SlangCompiler::MeshProgramResult shadow_mesh =
        compiler.compileMeshProgram("engine/shaders/shadow_mesh.slang");
    if (!shaderResourceBindingsMatch(shadow_mesh.layout, bindings, 6, sets,
                                     kinds)) {
      dumpBindings("shadow_mesh.slang", shadow_mesh.layout);
    }
    expect_true("shadow_mesh.slang layout",
                shaderResourceBindingsMatch(shadow_mesh.layout, bindings, 6, sets,
                                            kinds));
    expect_true("shadow_mesh.slang does not bind the Bindless set 1",
                !layoutUsesSet(shadow_mesh.layout, 1));
  }

  {
    uint32_t bindings[] = {0, 1, 2};
    uint32_t sets[] = {0, 0, 0};
    ShaderDescriptorKind kinds[] = {
        ShaderDescriptorKind::UniformBuffer, ShaderDescriptorKind::SampledImage,
        ShaderDescriptorKind::StorageBuffer};
    const SlangCompiler::ComputeProgramResult mark =
        compiler.compileComputeProgram("engine/shaders/vsm_page_mark.slang");
    if (!shaderResourceBindingsMatch(mark.layout, bindings, 3, sets, kinds)) {
      dumpBindings("vsm_page_mark.slang", mark.layout);
    }
    expect_true("vsm_page_mark.slang layout",
                shaderResourceBindingsMatch(mark.layout, bindings, 3, sets,
                                            kinds));
    expect_true("vsm_page_mark.slang does not bind the Bindless set 1",
                !layoutUsesSet(mark.layout, 1));
  }

  compiler.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  std::filesystem::remove_all(cache_root, ec);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
