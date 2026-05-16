#include "runtime/function/render/d3d12_backend/d3d12_render_backend.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/slang_compiler.h"

namespace Blunder::d3d12_backend {

D3D12RenderBackend::D3D12RenderBackend(const rhi::RenderBackendInitInfo& init) {
  (void)init;

  UINT factory_flags = 0;
#if defined(_DEBUG)
  factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
  Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
  if (FAILED(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)))) {
    LOG_FATAL("[D3D12RenderBackend] CreateDXGIFactory2 failed");
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  for (UINT i = 0;
       factory->EnumAdapterByGpuPreference(
           i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
           IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
       ++i) {
    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&m_device_com)))) {
      break;
    }
    adapter.Reset();
  }
  if (!m_device_com) {
    LOG_FATAL("[D3D12RenderBackend] D3D12CreateDevice failed");
  }

  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (FAILED(m_device_com->CreateCommandQueue(&queue_desc,
                                              IID_PPV_ARGS(&m_queue)))) {
    LOG_FATAL("[D3D12RenderBackend] CreateCommandQueue failed");
  }

  for (uint32_t i = 0; i < k_max_frames; ++i) {
    if (FAILED(m_device_com->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_command_allocators[i])))) {
      LOG_FATAL("[D3D12RenderBackend] CreateCommandAllocator failed");
    }
    m_frame_fence_values[i] = 0;
  }

  if (FAILED(m_device_com->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                       IID_PPV_ARGS(&m_fence)))) {
    LOG_FATAL("[D3D12RenderBackend] CreateFence failed");
  }
  m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!m_fence_event) {
    LOG_FATAL("[D3D12RenderBackend] CreateEvent failed");
  }

  m_slang_compiler = eastl::make_shared<SlangCompiler>();
  m_slang_compiler->initialize();

  m_device.bind(this);
  m_shader_compiler.bind(m_slang_compiler.get());
  m_frame_sync.bind(this);

  LOG_INFO("[D3D12RenderBackend] D3D12 skeleton initialized");
}

D3D12RenderBackend::~D3D12RenderBackend() {
  if (m_queue && m_fence) {
    waitForGpu((m_frame_index + k_max_frames - 1) % k_max_frames);
  }
  if (m_fence_event) {
    CloseHandle(m_fence_event);
    m_fence_event = nullptr;
  }
  if (m_slang_compiler) {
    m_slang_compiler->shutdown();
    m_slang_compiler.reset();
  }
}

void D3D12RenderBackend::executeCommandList(ID3D12GraphicsCommandList* list) {
  ID3D12CommandList* lists[] = {list};
  m_queue->ExecuteCommandLists(1, lists);
}

void D3D12RenderBackend::waitForGpu(uint32_t frame_index) {
  const UINT64 fence_value = m_frame_fence_values[frame_index % k_max_frames];
  if (fence_value == 0) {
    return;
  }
  if (m_fence->GetCompletedValue() < fence_value) {
    m_fence->SetEventOnCompletion(fence_value, m_fence_event);
    WaitForSingleObject(m_fence_event, INFINITE);
  }
}

void D3D12RenderBackend::resetFrameFence(uint32_t frame_index) {
  waitForGpu(frame_index);
  if (FAILED(commandAllocator()->Reset())) {
    LOG_FATAL("[D3D12RenderBackend] command allocator reset failed");
  }
}

void D3D12RenderBackend::signalFrameSubmitted(uint32_t frame_index) {
  ++m_fence_value;
  m_queue->Signal(m_fence.Get(), m_fence_value);
  m_frame_fence_values[frame_index % k_max_frames] = m_fence_value;
  m_frame_index = frame_index;
}

}  // namespace Blunder::d3d12_backend
