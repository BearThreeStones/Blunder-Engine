#include "runtime/function/render/d3d12_backend/d3d12_command_list.h"

#include "runtime/function/render/d3d12_backend/d3d12_render_backend.h"

namespace Blunder::d3d12_backend {

void D3D12CommandList::bind(D3D12RenderBackend* backend,
                            ID3D12GraphicsCommandList* list) {
  m_backend = backend;
  m_list = list;
}

void D3D12CommandList::begin() {
  if (m_list) {
    m_list->Reset(m_backend->commandAllocator(), nullptr);
  }
}

void D3D12CommandList::end() {
  if (m_list) {
    m_list->Close();
  }
}

void D3D12CommandList::submit() {
  if (m_backend && m_list) {
    m_backend->executeCommandList(m_list.Get());
  }
}

}  // namespace Blunder::d3d12_backend
