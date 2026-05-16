#include "runtime/function/render/d3d12_backend/d3d12_offscreen_target.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/d3d12_backend/d3d12_command_list.h"
#include "runtime/function/render/d3d12_backend/d3d12_render_backend.h"

namespace Blunder::d3d12_backend {

D3D12OffscreenTarget::~D3D12OffscreenTarget() { shutdown(); }

void D3D12OffscreenTarget::initialize(D3D12RenderBackend* backend,
                                      const rhi::OffscreenTargetDesc& desc) {
  m_backend = backend;
  m_width = desc.width;
  m_height = desc.height;
  createResources();
}

void D3D12OffscreenTarget::shutdown() {
  m_rtv_heap.Reset();
  m_dsv_heap.Reset();
  m_color.Reset();
  m_depth.Reset();
}

void D3D12OffscreenTarget::resize(uint32_t width, uint32_t height) {
  m_width = width;
  m_height = height;
  createResources();
}

rhi::Extent2D D3D12OffscreenTarget::extent() const {
  return {m_width, m_height};
}

rhi::PixelFormat D3D12OffscreenTarget::colorFormat() const {
  return rhi::PixelFormat::R8G8B8A8_UNORM;
}

void D3D12OffscreenTarget::createResources() {
  ASSERT(m_backend && m_width > 0 && m_height > 0);
  ID3D12Device* device = m_backend->nativeDevice();

  m_color.Reset();
  m_depth.Reset();
  m_rtv_heap.Reset();
  m_dsv_heap.Reset();

  D3D12_HEAP_PROPERTIES heap_props{};
  heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_RESOURCE_DESC color_desc{};
  color_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  color_desc.Width = m_width;
  color_desc.Height = m_height;
  color_desc.DepthOrArraySize = 1;
  color_desc.MipLevels = 1;
  color_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  color_desc.SampleDesc.Count = 1;
  color_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  D3D12_CLEAR_VALUE clear_value{};
  clear_value.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  clear_value.Color[0] = 0.1f;
  clear_value.Color[1] = 0.1f;
  clear_value.Color[2] = 0.1f;
  clear_value.Color[3] = 1.0f;

  if (FAILED(device->CreateCommittedResource(
          &heap_props, D3D12_HEAP_FLAG_NONE, &color_desc,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear_value,
          IID_PPV_ARGS(&m_color)))) {
    LOG_FATAL("[D3D12OffscreenTarget] failed to create color resource");
  }

  D3D12_RESOURCE_DESC depth_desc = color_desc;
  depth_desc.Format = DXGI_FORMAT_D32_FLOAT;
  depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  D3D12_CLEAR_VALUE depth_clear{};
  depth_clear.Format = DXGI_FORMAT_D32_FLOAT;
  depth_clear.DepthStencil.Depth = 1.0f;

  if (FAILED(device->CreateCommittedResource(
          &heap_props, D3D12_HEAP_FLAG_NONE, &depth_desc,
          D3D12_RESOURCE_STATE_DEPTH_WRITE, &depth_clear,
          IID_PPV_ARGS(&m_depth)))) {
    LOG_FATAL("[D3D12OffscreenTarget] failed to create depth resource");
  }

  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
  rtv_heap_desc.NumDescriptors = 1;
  rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  if (FAILED(device->CreateDescriptorHeap(&rtv_heap_desc,
                                          IID_PPV_ARGS(&m_rtv_heap)))) {
    LOG_FATAL("[D3D12OffscreenTarget] failed to create RTV heap");
  }

  D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = rtv_heap_desc;
  dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  if (FAILED(device->CreateDescriptorHeap(&dsv_heap_desc,
                                          IID_PPV_ARGS(&m_dsv_heap)))) {
    LOG_FATAL("[D3D12OffscreenTarget] failed to create DSV heap");
  }

  m_rtv_handle = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
  m_dsv_handle = m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
  device->CreateRenderTargetView(m_color.Get(), nullptr, m_rtv_handle);
  device->CreateDepthStencilView(m_depth.Get(), nullptr, m_dsv_handle);
}

void D3D12OffscreenTarget::beginRenderPass(
    rhi::ICommandList& command_list, const rhi::ClearValue* clears,
    uint32_t clear_count) {
  auto& d3d_list = static_cast<D3D12CommandList&>(command_list);
  ID3D12GraphicsCommandList* cmd = d3d_list.nativeList();
  ASSERT(cmd);

  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_color.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmd->ResourceBarrier(1, &barrier);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtv_handle;
  cmd->OMSetRenderTargets(1, &rtv, FALSE, &m_dsv_handle);

  if (clear_count > 0) {
    const float clear_color[] = {clears[0].color.r, clears[0].color.g,
                                 clears[0].color.b, clears[0].color.a};
    cmd->ClearRenderTargetView(m_rtv_handle, clear_color, 0, nullptr);
  }
  if (clear_count > 1) {
    cmd->ClearDepthStencilView(m_dsv_handle, D3D12_CLEAR_FLAG_DEPTH,
                               clears[1].depth_stencil.depth, 0, 0, nullptr);
  }

  D3D12_VIEWPORT viewport{};
  viewport.Width = static_cast<float>(m_width);
  viewport.Height = static_cast<float>(m_height);
  viewport.MaxDepth = 1.0f;
  D3D12_RECT scissor{0, 0, static_cast<LONG>(m_width),
                     static_cast<LONG>(m_height)};
  cmd->RSSetViewports(1, &viewport);
  cmd->RSSetScissorRects(1, &scissor);
}

void D3D12OffscreenTarget::endRenderPass(rhi::ICommandList& command_list) {
  (void)command_list;
}

void D3D12OffscreenTarget::transitionToCopySource(
    rhi::ICommandList& command_list) {
  auto& d3d_list = static_cast<D3D12CommandList&>(command_list);
  ID3D12GraphicsCommandList* cmd = d3d_list.nativeList();
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_color.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  cmd->ResourceBarrier(1, &barrier);
}

void D3D12OffscreenTarget::transitionToShaderRead(
    rhi::ICommandList& command_list) {
  (void)command_list;
}

void D3D12OffscreenTarget::copyColorToBuffer(rhi::ICommandList& command_list,
                                             rhi::IGpuBuffer& dst_buffer) {
  (void)command_list;
  (void)dst_buffer;
}

void D3D12OffscreenTarget::markPostRenderPassShaderRead() {}

}  // namespace Blunder::d3d12_backend
