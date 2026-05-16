#pragma once

#include "runtime/function/render/rhi/i_render_device.h"

namespace Blunder {

class VulkanAllocator;
class VulkanContext;

namespace vulkan_backend {

class VulkanRenderDevice final : public rhi::IRenderDevice {
 public:
  void bind(VulkanContext* context, VulkanAllocator* allocator);

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
  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
};

}  // namespace vulkan_backend
}  // namespace Blunder
