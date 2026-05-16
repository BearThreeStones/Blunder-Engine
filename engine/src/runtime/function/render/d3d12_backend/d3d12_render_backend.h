#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "EASTL/array.h"
#include "EASTL/shared_ptr.h"

#include "runtime/function/render/d3d12_backend/d3d12_frame_sync.h"
#include "runtime/function/render/d3d12_backend/d3d12_render_device.h"
#include "runtime/function/render/d3d12_backend/d3d12_shader_compiler.h"
#include "runtime/function/render/rhi/i_render_backend.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder {

class SlangCompiler;

namespace d3d12_backend {

class D3D12RenderBackend final : public rhi::IRenderBackend {
 public:
  explicit D3D12RenderBackend(const rhi::RenderBackendInitInfo& init);
  ~D3D12RenderBackend() override;

  rhi::RenderBackendType type() const override {
    return rhi::RenderBackendType::D3D12;
  }
  rhi::IRenderDevice& device() override { return m_device; }
  const rhi::IRenderDevice& device() const override { return m_device; }
  rhi::IShaderCompiler& shaderCompiler() override { return m_shader_compiler; }
  const rhi::IShaderCompiler& shaderCompiler() const override {
    return m_shader_compiler;
  }
  rhi::IFrameSync& frameSync() override { return m_frame_sync; }
  const rhi::IFrameSync& frameSync() const override { return m_frame_sync; }

  ID3D12Device* nativeDevice() const { return m_device_com.Get(); }
  ID3D12CommandQueue* nativeQueue() const { return m_queue.Get(); }
  ID3D12CommandAllocator* commandAllocator() const {
    return m_command_allocators[m_frame_index % k_max_frames].Get();
  }

  void executeCommandList(ID3D12GraphicsCommandList* list);
  void waitForGpu(uint32_t frame_index);
  void resetFrameFence(uint32_t frame_index);
  void signalFrameSubmitted(uint32_t frame_index);

 private:
  eastl::shared_ptr<SlangCompiler> m_slang_compiler;
  Microsoft::WRL::ComPtr<ID3D12Device> m_device_com;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
  Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
  UINT64 m_fence_value{0};
  HANDLE m_fence_event{nullptr};
  static constexpr uint32_t k_max_frames = VulkanSync::k_max_frames_in_flight;
  eastl::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, k_max_frames>
      m_command_allocators{};
  eastl::array<UINT64, k_max_frames> m_frame_fence_values{};
  uint32_t m_frame_index{0};

  D3D12RenderDevice m_device;
  D3D12ShaderCompiler m_shader_compiler;
  D3D12FrameSync m_frame_sync;
};

}  // namespace d3d12_backend
}  // namespace Blunder
