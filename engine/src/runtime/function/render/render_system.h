#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/shared_ptr.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

namespace Blunder {

class WindowSystem;
class Event;

class SlangCompiler;
class EditorCamera;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class VulkanPipeline;
class VulkanSwapchain;
class VulkanSync;

struct RenderSystemInitInfo {
  WindowSystem* window_system{nullptr};
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
  void onEvent(Event& event);

  VkRenderPass getRenderPass() const;

  VulkanContext* getVulkanContext() const { return m_context.get(); }
  VulkanSwapchain* getSwapchain() const { return m_swapchain.get(); }
  VulkanAllocator* getAllocator() const { return m_allocator.get(); }
  EditorCamera* getEditorCamera() const { return m_editor_camera.get(); }

 private:
  void recreateSwapchain();

  WindowSystem* m_window_system{nullptr};
  eastl::shared_ptr<SlangCompiler> m_slang_compiler;
  eastl::shared_ptr<VulkanContext> m_context;
  eastl::shared_ptr<VulkanAllocator> m_allocator;
  eastl::shared_ptr<VulkanSwapchain> m_swapchain;
  eastl::shared_ptr<VulkanSync> m_sync;
  eastl::shared_ptr<VulkanPipeline> m_pipeline;
  eastl::unique_ptr<EditorCamera> m_editor_camera;
  eastl::unique_ptr<VulkanBuffer> m_vertex_buffer;
  eastl::unique_ptr<VulkanBuffer> m_index_buffer;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_uniform_buffers;
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  eastl::vector<VkDescriptorSet> m_descriptor_sets;
  uint32_t m_current_frame{0};
  float m_elapsed_time{0.0f};
};

}  // namespace Blunder
