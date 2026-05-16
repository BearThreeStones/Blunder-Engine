#pragma once

#include "EASTL/shared_ptr.h"
#include "EASTL/unique_ptr.h"

#include "runtime/function/render/rhi/i_render_backend.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/vulkan_backend/vulkan_frame_sync.h"
#include "runtime/function/render/vulkan_backend/vulkan_render_device.h"
#include "runtime/function/render/vulkan_backend/vulkan_shader_compiler.h"

namespace Blunder {

class SlangCompiler;
class VulkanAllocator;
class VulkanContext;
class VulkanSync;

namespace vulkan_backend {

class VulkanRenderBackend final : public rhi::IRenderBackend {
 public:
  explicit VulkanRenderBackend(const rhi::RenderBackendInitInfo& init);
  ~VulkanRenderBackend() override;

  rhi::RenderBackendType type() const override {
    return rhi::RenderBackendType::Vulkan;
  }
  rhi::IRenderDevice& device() override { return m_device; }
  const rhi::IRenderDevice& device() const override { return m_device; }
  rhi::IShaderCompiler& shaderCompiler() override { return m_shader_compiler; }
  const rhi::IShaderCompiler& shaderCompiler() const override {
    return m_shader_compiler;
  }
  rhi::IFrameSync& frameSync() override { return m_frame_sync; }
  const rhi::IFrameSync& frameSync() const override { return m_frame_sync; }

  VulkanContext* nativeVulkanContext() const { return m_context.get(); }
  VulkanAllocator* nativeAllocator() const { return m_allocator.get(); }
  VulkanSync* nativeSync() const { return m_sync.get(); }
  SlangCompiler* nativeSlangCompiler() const { return m_slang_compiler.get(); }

 private:
  eastl::shared_ptr<SlangCompiler> m_slang_compiler;
  eastl::shared_ptr<VulkanContext> m_context;
  eastl::shared_ptr<VulkanAllocator> m_allocator;
  eastl::shared_ptr<VulkanSync> m_sync;
  VulkanRenderDevice m_device;
  VulkanShaderCompiler m_shader_compiler;
  VulkanFrameSync m_frame_sync;
};

}  // namespace vulkan_backend
}  // namespace Blunder
