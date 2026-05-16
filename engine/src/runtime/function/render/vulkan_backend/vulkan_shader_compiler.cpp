#include "runtime/function/render/vulkan_backend/vulkan_shader_compiler.h"

#include <slang.h>

#include "runtime/function/render/slang/slang_compiler.h"

namespace Blunder::vulkan_backend {

namespace {

int toSlangStage(rhi::ShaderStage stage) {
  switch (stage) {
    case rhi::ShaderStage::Vertex:
      return SLANG_STAGE_VERTEX;
    case rhi::ShaderStage::Fragment:
      return SLANG_STAGE_FRAGMENT;
    default:
      return SLANG_STAGE_VERTEX;
  }
}

}  // namespace

void VulkanShaderCompiler::bind(SlangCompiler* compiler) { m_compiler = compiler; }

rhi::ShaderBytecode VulkanShaderCompiler::compile(const char* source_path,
                                                  const char* entry_point,
                                                  rhi::ShaderStage stage) {
  rhi::ShaderBytecode result;
  if (!m_compiler) {
    return result;
  }
  const SlangCompiler::ShaderResult spirv =
      m_compiler->compileShader(source_path, entry_point, toSlangStage(stage));
  result.code = spirv.spirv_code;
  result.entry_point_name = spirv.entry_point_name;
  result.format = rhi::ShaderBytecodeFormat::Spirv;
  result.stage = stage;
  return result;
}

}  // namespace Blunder::vulkan_backend
