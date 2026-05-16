#pragma once

#include "runtime/function/render/rhi/rhi_types.h"

namespace Blunder::rhi {

class ICommandList;
class IGpuBuffer;

class IOffscreenRenderTarget {
 public:
  virtual ~IOffscreenRenderTarget() = default;

  virtual void resize(uint32_t width, uint32_t height) = 0;
  virtual Extent2D extent() const = 0;
  virtual PixelFormat colorFormat() const = 0;

  virtual void beginRenderPass(ICommandList& command_list,
                               const ClearValue* clears,
                               uint32_t clear_count) = 0;
  virtual void endRenderPass(ICommandList& command_list) = 0;
  virtual void transitionToCopySource(ICommandList& command_list) = 0;
  virtual void transitionToShaderRead(ICommandList& command_list) = 0;
  virtual void copyColorToBuffer(ICommandList& command_list,
                                 IGpuBuffer& dst_buffer) = 0;
  virtual void markPostRenderPassShaderRead() = 0;
};

}  // namespace Blunder::rhi
