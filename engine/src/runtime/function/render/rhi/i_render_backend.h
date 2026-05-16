#pragma once

#include "runtime/function/render/rhi/rhi_types.h"

namespace Blunder::rhi {

class IRenderDevice;
class IShaderCompiler;
class IFrameSync;

class IRenderBackend {
 public:
  virtual ~IRenderBackend() = default;

  virtual RenderBackendType type() const = 0;
  virtual IRenderDevice& device() = 0;
  virtual const IRenderDevice& device() const = 0;
  virtual IShaderCompiler& shaderCompiler() = 0;
  virtual const IShaderCompiler& shaderCompiler() const = 0;
  virtual IFrameSync& frameSync() = 0;
  virtual const IFrameSync& frameSync() const = 0;
};

}  // namespace Blunder::rhi
