#pragma once

#include "EASTL/unique_ptr.h"

#include <cstdint>

#include "runtime/function/render/rhi/rhi_desc.h"

namespace Blunder {

class Texture2DAsset;

namespace rhi {

class ICommandList;
class IGpuBuffer;
class IGpuTexture;
class IOffscreenRenderTarget;

class IRenderDevice {
 public:
  virtual ~IRenderDevice() = default;

  virtual void initialize(const RenderDeviceDesc& desc) = 0;
  virtual void shutdown() = 0;

  virtual eastl::unique_ptr<IOffscreenRenderTarget> createOffscreenTarget(
      const OffscreenTargetDesc& desc) = 0;
  virtual eastl::unique_ptr<IGpuBuffer> createBuffer(const BufferDesc& desc) = 0;
  virtual eastl::unique_ptr<IGpuTexture> createTextureFromAsset(
      const Texture2DAsset* asset) = 0;
  virtual eastl::unique_ptr<ICommandList> beginImmediateCommandList() = 0;
};

}  // namespace rhi
}  // namespace Blunder
