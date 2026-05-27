#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "runtime/function/render/overlay/overlay_base.h"

namespace Blunder {

class OffscreenRenderTarget;
class OverlayLineTargets;
class SlangCompiler;
class VulkanAllocator;
class VulkanContext;

namespace rhi {
class IOffscreenRenderTarget;
}  // namespace rhi

/// Fullscreen composite of MRT overlay lines onto the main offscreen color.
class OverlayAntiAliasing final : public Overlay {
 public:
  OverlayAntiAliasing() = default;
  ~OverlayAntiAliasing();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                  rhi::IOffscreenRenderTarget* offscreen,
                  SlangCompiler* compiler,
                  OverlayLineTargets* line_targets);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void apply(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen,
             const OverlayState& state);

 private:
  void createRenderPass();
  void destroyRenderPass();
  void createPipeline();
  void destroyPipeline();
  void createDescriptorResources();
  void destroyDescriptorResources();
  void createSceneSnapshotResources();
  void destroySceneSnapshotResources();
  void copySceneSnapshot(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen);
  void writeDescriptors();

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  SlangCompiler* m_compiler{nullptr};
  OverlayLineTargets* m_line_targets{nullptr};

  uint32_t m_width{0};
  uint32_t m_height{0};

  VkRenderPass m_render_pass{VK_NULL_HANDLE};
  VkSampler m_linear_sampler{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descriptor_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};
  eastl::unique_ptr<class VulkanBuffer> m_uniform_buffer;

  VkImage m_scene_snapshot_image{VK_NULL_HANDLE};
  VmaAllocation m_scene_snapshot_allocation{VK_NULL_HANDLE};
  VkImageView m_scene_snapshot_view{VK_NULL_HANDLE};
  VkImageLayout m_scene_snapshot_layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

}  // namespace Blunder
