#pragma once

#include <cstdint>

#include "runtime/function/render/rhi/rhi_desc.h"

namespace Blunder::rhi {

class ICommandList;
class IOffscreenRenderTarget;

class IGraphicsPipeline {
 public:
  virtual ~IGraphicsPipeline() = default;

  virtual void initialize(IOffscreenRenderTarget& render_target,
                          const GraphicsPipelineDesc& desc) = 0;
  virtual void shutdown() = 0;

  virtual ICommandList* commandList(uint32_t frame_index) = 0;
  virtual const ICommandList* commandList(uint32_t frame_index) const = 0;
};

}  // namespace Blunder::rhi
