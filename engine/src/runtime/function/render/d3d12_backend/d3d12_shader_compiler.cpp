#include "runtime/function/render/d3d12_backend/d3d12_shader_compiler.h"

#include <slang.h>

#include "runtime/function/render/slang/slang_compiler.h"

namespace Blunder::d3d12_backend {

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

void D3D12ShaderCompiler::bind(SlangCompiler* compiler) { m_compiler = compiler; }

rhi::ShaderBytecode D3D12ShaderCompiler::compile(const char* source_path,
                                                 const char* entry_point,
                                                 rhi::ShaderStage stage) {
  rhi::ShaderBytecode result;
  if (!m_compiler) {
    return result;
  }
  const SlangCompiler::ShaderResult dxil = m_compiler->compileShaderDxil(
      source_path, entry_point, toSlangStage(stage));
  result.code = dxil.dxil_code;
  result.entry_point_name = dxil.entry_point_name;
  result.format = rhi::ShaderBytecodeFormat::Dxil;
  result.stage = stage;
  return result;
}

}  // namespace Blunder::d3d12_backend
