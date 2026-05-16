#pragma once

#include "runtime/function/render/rhi/i_shader_compiler.h"

namespace Blunder {

class SlangCompiler;

namespace vulkan_backend {

class VulkanShaderCompiler final : public rhi::IShaderCompiler {
 public:
  void bind(SlangCompiler* compiler);

  SlangCompiler* nativeCompiler() const { return m_compiler; }

  rhi::ShaderBytecode compile(const char* source_path, const char* entry_point,
                              rhi::ShaderStage stage) override;

 private:
  SlangCompiler* m_compiler{nullptr};
};

}  // namespace vulkan_backend
}  // namespace Blunder
