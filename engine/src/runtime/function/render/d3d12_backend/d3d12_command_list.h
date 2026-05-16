#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "runtime/function/render/rhi/i_command_list.h"

namespace Blunder::d3d12_backend {

class D3D12RenderBackend;

class D3D12CommandList final : public rhi::ICommandList {
 public:
  void bind(D3D12RenderBackend* backend, ID3D12GraphicsCommandList* list);

  ID3D12GraphicsCommandList* nativeList() const { return m_list.Get(); }

  void begin() override;
  void end() override;
  void submit() override;

 private:
  D3D12RenderBackend* m_backend{nullptr};
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_list;
};

}  // namespace Blunder::d3d12_backend
