#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/shared_ptr.h"
#include "EASTL/unique_ptr.h"

struct GLFWwindow;

namespace Blunder {

class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class VulkanPipeline;
class VulkanSwapchain;
class VulkanSync;

struct RenderSystemInitInfo {
  GLFWwindow* window{nullptr};
  bool enable_validation{true};
};

class RenderSystem final {
 public:
  RenderSystem();
  ~RenderSystem();

  void initialize(const RenderSystemInitInfo& info);
  void shutdown();
  void tick(float delta_time,
            void (*overlay_fn)(VkCommandBuffer) = nullptr);

  VkRenderPass getRenderPass() const;

  VulkanContext* getVulkanContext() const { return m_context.get(); }
  VulkanSwapchain* getSwapchain() const { return m_swapchain.get(); }
  VulkanAllocator* getAllocator() const { return m_allocator.get(); }

 private:
  void recreateSwapchain();

  GLFWwindow* m_window{nullptr};
  eastl::shared_ptr<SlangCompiler> m_slang_compiler;
  eastl::shared_ptr<VulkanContext> m_context;
  eastl::shared_ptr<VulkanAllocator> m_allocator;
  eastl::shared_ptr<VulkanSwapchain> m_swapchain;
  eastl::shared_ptr<VulkanSync> m_sync;
  eastl::shared_ptr<VulkanPipeline> m_pipeline;
  eastl::unique_ptr<VulkanBuffer> m_vertex_buffer;
  uint32_t m_current_frame{0};
};

}  // namespace Blunder
