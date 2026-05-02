#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
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

/// RGBA8 pixels (row-major). Width/height must match the swapchain drawable size when passed to tick().
struct UiCpuTextureView {
  const uint8_t* pixels_rgba{nullptr};
  uint32_t width{0};
  uint32_t height{0};
};

class RenderSystem final {
 public:
  RenderSystem();
  ~RenderSystem();

  void initialize(const RenderSystemInitInfo& info);
  void shutdown();
  void tick(float delta_time, const UiCpuTextureView* ui_overlay = nullptr,
            void (*overlay_fn)(VkCommandBuffer) = nullptr);
  void onEvent(Event& event);

  VkRenderPass getRenderPass() const;

  VulkanContext* getVulkanContext() const { return m_context.get(); }
  VulkanSwapchain* getSwapchain() const { return m_swapchain.get(); }
  VulkanAllocator* getAllocator() const { return m_allocator.get(); }
  EditorCamera* getEditorCamera() const { return m_editor_camera.get(); }

 private:
  void recreateSwapchain();
  void createUiOverlayResources(VkExtent2D extent);
  void destroyUiOverlayResources();
  void ensureUiTexture(VkExtent2D extent);
  void destroyUiTextureOnly();

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

  VkDescriptorSetLayout m_ui_descriptor_set_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_ui_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_ui_pipeline{VK_NULL_HANDLE};
  VkSampler m_ui_sampler{VK_NULL_HANDLE};
  VkDescriptorPool m_ui_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_ui_descriptor_set{VK_NULL_HANDLE};
  VkImage m_ui_image{VK_NULL_HANDLE};
  VmaAllocation m_ui_image_allocation{VK_NULL_HANDLE};
  VkImageView m_ui_image_view{VK_NULL_HANDLE};
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_ui_staging_buffers;
  uint32_t m_ui_alloc_width{0};
  uint32_t m_ui_alloc_height{0};
  bool m_ui_first_transfer{true};
};

}  // namespace Blunder
