#pragma once

#include "EASTL/unique_ptr.h"

#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/rhi_desc.h"

namespace Blunder {

class OffscreenRenderTarget;
class VulkanAllocator;
class VulkanContext;

namespace vulkan_backend {

class VulkanOffscreenTarget final : public rhi::IOffscreenRenderTarget {
 public:
  VulkanOffscreenTarget() = default;
  ~VulkanOffscreenTarget() override;

  void initialize(VulkanContext* context, VulkanAllocator* allocator,
                  const rhi::OffscreenTargetDesc& desc);
  void shutdown();

  OffscreenRenderTarget* nativeTarget() const { return m_target.get(); }

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
  eastl::unique_ptr<OffscreenRenderTarget> m_target;
};

}  // namespace vulkan_backend
}  // namespace Blunder
