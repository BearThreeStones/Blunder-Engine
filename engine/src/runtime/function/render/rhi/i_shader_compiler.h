#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/render/rhi/rhi_types.h"

namespace Blunder::rhi {

struct ShaderBytecode {
  eastl::vector<uint8_t> code;
  eastl::string entry_point_name;
  ShaderBytecodeFormat format{ShaderBytecodeFormat::Spirv};
  ShaderStage stage{ShaderStage::Vertex};
};

class IShaderCompiler {
 public:
  virtual ~IShaderCompiler() = default;

  virtual ShaderBytecode compile(const char* source_path,
                                 const char* entry_point,
                                 ShaderStage stage) = 0;
};

}  // namespace Blunder::rhi
