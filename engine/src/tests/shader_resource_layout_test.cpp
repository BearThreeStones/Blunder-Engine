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
  uint32_t expected_count = 0;
  Blunder::fillSequentialExpectedBindings(expected, &expected_count, count);
  return Blunder::shaderResourceBindingsMatch(layout, expected, expected_count);
}

void dumpBindings(const char* label,
                  const Blunder::ShaderResourceLayout& layout) {
  const uint32_t n = layout.count > Blunder::k_max_expected_descriptor_bindings
                         ? Blunder::k_max_expected_descriptor_bindings
                         : layout.count;
  std::fprintf(stderr, "%s (count=%u):", label, layout.count);
  for (uint32_t i = 0; i < n; ++i) {
    std::fprintf(stderr, " %u:%u", layout.bindings[i].set,
                 layout.bindings[i].binding);
  }
  std::fprintf(stderr, "\n");
}

bool layoutMatchesPbr(const Blunder::ShaderResourceLayout& layout, bool skinned) {
  uint32_t expected[Blunder::k_max_expected_descriptor_bindings];
  uint32_t sets[Blunder::k_max_expected_descriptor_bindings];
  uint32_t expected_count = 0;
  Blunder::fillPbrMeshExpectedBindings(expected, sets, &expected_count, skinned);
  return Blunder::shaderResourceBindingsMatch(layout, expected, expected_count,
                                              sets);
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
  }

  {
    uint32_t bindings[k_max_expected_descriptor_bindings];
    uint32_t sets[k_max_expected_descriptor_bindings];
    uint32_t count = 0;
    fillPbrMeshExpectedBindings(bindings, sets, &count, false);
    expect_true("unskinned pbr expected count",
                count == k_pbr_descriptor_binding_count);
    expect_true("unskinned pbr set0 ubo", sets[0] == 0 && bindings[0] == 0);
    expect_true("unskinned pbr set0 shadow image",
                sets[1] == 0 && bindings[1] == 1);
    expect_true("unskinned pbr set0 shadow sampler",
                sets[2] == 0 && bindings[2] == 2);
    expect_true("unskinned pbr set1 textures", sets[3] == 1 && bindings[3] == 0);
    expect_true("unskinned pbr set1 samplers", sets[4] == 1 && bindings[4] == 1);

    fillPbrMeshExpectedBindings(bindings, sets, &count, true);
    expect_true("skinned pbr expected count",
                count == k_skinned_pbr_descriptor_binding_count);
    expect_true("skinned pbr bone ubo", sets[3] == 0 && bindings[3] == 3);
    expect_true("skinned pbr set1 textures", sets[4] == 1 && bindings[4] == 0);
    expect_true("skinned pbr set1 samplers", sets[5] == 1 && bindings[5] == 1);
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

  compiler.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  std::filesystem::remove_all(cache_root, ec);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
