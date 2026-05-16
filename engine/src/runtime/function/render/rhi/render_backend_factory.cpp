#include "runtime/function/render/rhi/render_backend_factory.h"

#include "runtime/core/base/macro.h"

#if defined(_WIN32)
#  include "runtime/function/render/d3d12_backend/d3d12_render_backend.h"
#endif
#include "runtime/function/render/vulkan_backend/vulkan_render_backend.h"

namespace Blunder::rhi {

RenderBackendType RenderBackendFactory::defaultBackendType() {
#if defined(BLUNDER_RENDER_BACKEND_D3D12)
  return RenderBackendType::D3D12;
#else
  return RenderBackendType::Vulkan;
#endif
}

eastl::unique_ptr<IRenderBackend> RenderBackendFactory::create(
    RenderBackendType type, const RenderBackendInitInfo& init) {
  switch (type) {
    case RenderBackendType::Vulkan:
      return eastl::make_unique<vulkan_backend::VulkanRenderBackend>(init);
#if defined(_WIN32)
    case RenderBackendType::D3D12:
#  if defined(BLUNDER_RENDER_BACKEND_D3D12)
      return eastl::make_unique<d3d12_backend::D3D12RenderBackend>(init);
#  else
      LOG_FATAL(
          "[RenderBackendFactory] rebuild with -DBLUNDER_RENDER_BACKEND=d3d12 "
          "to use the D3D12 skeleton backend");
      return nullptr;
#  endif
#else
    case RenderBackendType::D3D12:
      LOG_FATAL("[RenderBackendFactory] D3D12 backend is only supported on Windows");
      return nullptr;
#endif
    default:
      LOG_FATAL("[RenderBackendFactory] unsupported backend type");
      return nullptr;
  }
}

eastl::unique_ptr<IRenderBackend> RenderBackendFactory::createFromSettings(
    const RenderBackendInitInfo& init) {
  return create(defaultBackendType(), init);
}

}  // namespace Blunder::rhi
