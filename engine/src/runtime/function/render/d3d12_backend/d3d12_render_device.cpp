#include "runtime/function/render/d3d12_backend/d3d12_render_device.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/rhi/i_command_list.h"
#include "runtime/function/render/rhi/i_gpu_buffer.h"
#include "runtime/function/render/rhi/i_gpu_texture.h"
#include "runtime/function/render/d3d12_backend/d3d12_offscreen_target.h"
#include "runtime/function/render/d3d12_backend/d3d12_render_backend.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder::d3d12_backend {

void D3D12RenderDevice::bind(D3D12RenderBackend* backend) {
  m_backend = backend;
}

void D3D12RenderDevice::initialize(const rhi::RenderDeviceDesc& desc) {
  (void)desc;
}

void D3D12RenderDevice::shutdown() {}

eastl::unique_ptr<rhi::IOffscreenRenderTarget>
D3D12RenderDevice::createOffscreenTarget(const rhi::OffscreenTargetDesc& desc) {
  auto target = eastl::make_unique<D3D12OffscreenTarget>();
  target->initialize(m_backend, desc);
  return target;
}

eastl::unique_ptr<rhi::IGpuBuffer> D3D12RenderDevice::createBuffer(
    const rhi::BufferDesc& desc) {
  (void)desc;
  LOG_FATAL("[D3D12RenderDevice] createBuffer is not implemented in P0 skeleton");
  return nullptr;
}

eastl::unique_ptr<rhi::IGpuTexture> D3D12RenderDevice::createTextureFromAsset(
    const Texture2DAsset* asset) {
  (void)asset;
  LOG_FATAL(
      "[D3D12RenderDevice] createTextureFromAsset is not implemented in P0");
  return nullptr;
}

eastl::unique_ptr<rhi::ICommandList>
D3D12RenderDevice::beginImmediateCommandList() {
  LOG_FATAL(
      "[D3D12RenderDevice] beginImmediateCommandList is not implemented in P0");
  return nullptr;
}

}  // namespace Blunder::d3d12_backend
