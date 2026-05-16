#pragma once

#include "runtime/function/render/rhi/i_render_device.h"

namespace Blunder::d3d12_backend {

class D3D12RenderBackend;

class D3D12RenderDevice final : public rhi::IRenderDevice {
 public:
  void bind(D3D12RenderBackend* backend);

  void initialize(const rhi::RenderDeviceDesc& desc) override;
  void shutdown() override;

  eastl::unique_ptr<rhi::IOffscreenRenderTarget> createOffscreenTarget(
      const rhi::OffscreenTargetDesc& desc) override;
  eastl::unique_ptr<rhi::IGpuBuffer> createBuffer(
      const rhi::BufferDesc& desc) override;
  eastl::unique_ptr<rhi::IGpuTexture> createTextureFromAsset(
      const Texture2DAsset* asset) override;
  eastl::unique_ptr<rhi::ICommandList> beginImmediateCommandList() override;

 private:
  D3D12RenderBackend* m_backend{nullptr};
};

}  // namespace Blunder::d3d12_backend
