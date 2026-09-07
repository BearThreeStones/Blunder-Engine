#include "runtime/function/render/vulkan/vulkan_shader.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/slang_compiler.h"

namespace Blunder {

namespace {

VulkanShader::ShaderStage createStageFromSpirv(
    VkDevice device, const eastl::vector<uint8_t>& spirv_code,
    VkShaderStageFlagBits stage_flags, const eastl::string& entry_point) {
  ASSERT(spirv_code.size() % sizeof(uint32_t) == 0);

  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = spirv_code.size();
  create_info.pCode = reinterpret_cast<const uint32_t*>(spirv_code.data());

  VkShaderModule shader_module = VK_NULL_HANDLE;
  const VkResult vk_result =
      vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
  if (vk_result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanShader] vkCreateShaderModule failed for entry '{}': {}",
        entry_point.c_str(), static_cast<int>(vk_result));
  }

  VulkanShader::ShaderStage stage;
  stage.module = shader_module;
  stage.stage_flags = stage_flags;
  stage.entry_point = entry_point;
  return stage;
}

}  // namespace

VulkanShader::GraphicsProgram VulkanShader::loadGraphicsProgramFromSlang(
    VkDevice device, SlangCompiler* compiler, const char* slang_path,
    const char* vertex_entry, const char* fragment_entry) {
  ASSERT(device != VK_NULL_HANDLE);
  ASSERT(compiler);
  ASSERT(slang_path);
  ASSERT(vertex_entry);
  ASSERT(fragment_entry);

  const SlangCompiler::GraphicsProgramResult program =
      compiler->compileGraphicsProgram(slang_path, vertex_entry, fragment_entry);

  GraphicsProgram loaded;
  loaded.layout = program.layout;
  loaded.stages.reserve(2);
  loaded.stages.push_back(createStageFromSpirv(
      device, program.vertex.spirv_code, VK_SHADER_STAGE_VERTEX_BIT,
      program.vertex.entry_point_name));
  loaded.stages.push_back(createStageFromSpirv(
      device, program.fragment.spirv_code, VK_SHADER_STAGE_FRAGMENT_BIT,
      program.fragment.entry_point_name));
  return loaded;
}

eastl::vector<VulkanShader::ShaderStage> VulkanShader::loadFromSlang(
    VkDevice device, SlangCompiler* compiler, const char* slang_path,
    const eastl::vector<EntryPointSpec>& entries) {
  ASSERT(device != VK_NULL_HANDLE);
  ASSERT(compiler);
  ASSERT(slang_path);
  ASSERT(!entries.empty());

  eastl::vector<ShaderStage> stages;
  stages.reserve(entries.size());

  for (const EntryPointSpec& spec : entries) {
    SlangCompiler::ShaderResult result =
        compiler->compileShader(slang_path, spec.name, spec.slang_stage);
    stages.push_back(createStageFromSpirv(device, result.spirv_code,
                                          spec.stage_flags,
                                          result.entry_point_name));
  }

  return stages;
}

void VulkanShader::destroyShaderModule(VkDevice device,
                                       VkShaderModule* shader_module) {
  ASSERT(shader_module);
  if (*shader_module != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device, *shader_module, nullptr);
    *shader_module = VK_NULL_HANDLE;
  }
}

}  // namespace Blunder
