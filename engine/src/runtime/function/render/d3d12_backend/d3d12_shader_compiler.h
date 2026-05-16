#pragma once

#include "runtime/function/render/rhi/i_shader_compiler.h"

namespace Blunder {

class SlangCompiler;

namespace d3d12_backend {

class D3D12ShaderCompiler final : public rhi::IShaderCompiler {
 public:
  void bind(SlangCompiler* compiler);

  rhi::ShaderBytecode compile(const char* source_path, const char* entry_point,
                              rhi::ShaderStage stage) override;

 private:
  SlangCompiler* m_compiler{nullptr};
};

}  // namespace d3d12_backend
}  // namespace Blunder
