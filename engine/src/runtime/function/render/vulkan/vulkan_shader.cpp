#include "runtime/function/render/vulkan/vulkan_shader.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/slang_compiler.h"

namespace Blunder {

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

    // SPIR-V 必须对齐到 uint32_t
    ASSERT(result.spirv_code.size() % sizeof(uint32_t) == 0);

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = result.spirv_code.size();
    create_info.pCode =
        reinterpret_cast<const uint32_t*>(result.spirv_code.data());

    VkShaderModule shader_module = VK_NULL_HANDLE;
    const VkResult vk_result =
        vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
    if (vk_result != VK_SUCCESS) {
      // 清理已经创建的模块
      // 将 SPIR-V 字节码编译并链接为 GPU
      // 可执行的机器码的操作要等到图形管线创建时才会发生，
      // 这意味着一旦管线创建完成就可以销毁着色器模块
      for (ShaderStage& s : stages) {
        destroyShaderModule(device, &s.module);
      }
      LOG_FATAL(
          "[VulkanShader::loadFromSlang] vkCreateShaderModule failed for "
          "entry '{}': {}",
          spec.name, static_cast<int>(vk_result));
    }

    ShaderStage stage;
    stage.module = shader_module;
    stage.stage_flags = spec.stage_flags;
    stage.entry_point = result.entry_point_name;
    stages.push_back(eastl::move(stage));
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
