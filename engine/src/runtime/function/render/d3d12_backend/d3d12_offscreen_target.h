#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/rhi_desc.h"

namespace Blunder::d3d12_backend {

class D3D12RenderBackend;

class D3D12OffscreenTarget final : public rhi::IOffscreenRenderTarget {
 public:
  D3D12OffscreenTarget() = default;
  ~D3D12OffscreenTarget() override;

  void initialize(D3D12RenderBackend* backend,
                  const rhi::OffscreenTargetDesc& desc);
  void shutdown();

  void resize(uint32_t width, uint32_t height) override;
  rhi::Extent2D extent() const override;
  rhi::PixelFormat colorFormat() const override;

  void beginRenderPass(rhi::ICommandList& command_list,
                       const rhi::ClearValue* clears,
                       uint32_t clear_count) override;
  void endRenderPass(rhi::ICommandList& command_list) override;
  void transitionToCopySource(rhi::ICommandList& command_list) override;
  void transitionToShaderRead(rhi::ICommandList& command_list) override;
  void copyColorToBuffer(rhi::ICommandList& command_list,
                         rhi::IGpuBuffer& dst_buffer) override;
  void markPostRenderPassShaderRead() override;

 private:
  void createResources();

  D3D12RenderBackend* m_backend{nullptr};
  uint32_t m_width{0};
  uint32_t m_height{0};
  Microsoft::WRL::ComPtr<ID3D12Resource> m_color;
  Microsoft::WRL::ComPtr<ID3D12Resource> m_depth;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsv_heap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_rtv_handle{};
  D3D12_CPU_DESCRIPTOR_HANDLE m_dsv_handle{};
};

}  // namespace Blunder::d3d12_backend
