#pragma once

#include <cstdint>

#include <glm/ext/matrix_float4x4.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"

#include "runtime/function/render/blinn_phong_editor_settings.h"

namespace Blunder {

class OffscreenRenderTarget;
class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class VulkanTexture;

/// Screen-space ambient occlusion post-process (depth reconstruction + composite).
class SsaOPass final {
 public:
  SsaOPass() = default;
  ~SsaOPass();

  void initialize(VulkanContext* context, VulkanAllocator* allocator,
                  SlangCompiler* compiler);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  void apply(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen,
             const BlinnPhongEditorSettings& settings,
             const glm::mat4& projection, float near_clip, float far_clip);

 private:
  void createNoiseTexture();
  void destroyAoResources();
  void createAoResources();
  void createRenderPasses();
  void destroyRenderPasses();
  void createPipelines();
  void destroyPipelines();
  void createDescriptorResources();
  void destroyDescriptorResources();
  void writeGenerateDescriptors(OffscreenRenderTarget* offscreen);
  void writeApplyDescriptors();
  void copySceneSnapshot(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen);
  void uploadUniforms(const BlinnPhongEditorSettings& settings,
                      const glm::mat4& projection, float near_clip,
                      float far_clip, uint32_t width, uint32_t height);

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  SlangCompiler* m_compiler{nullptr};

  uint32_t m_width{0};
  uint32_t m_height{0};

  VkRenderPass m_ao_render_pass{VK_NULL_HANDLE};
  VkRenderPass m_composite_render_pass{VK_NULL_HANDLE};
  VkFramebuffer m_ao_framebuffer{VK_NULL_HANDLE};
  VkFramebuffer m_composite_framebuffer{VK_NULL_HANDLE};

  VkImage m_ao_image{VK_NULL_HANDLE};
  VmaAllocation m_ao_allocation{VK_NULL_HANDLE};
  VkImageView m_ao_image_view{VK_NULL_HANDLE};
  VkImageLayout m_ao_layout{VK_IMAGE_LAYOUT_UNDEFINED};

  // Forward color copy sampled by the composite pass (avoids feedback on offscreen).
  VkImage m_scene_snapshot_image{VK_NULL_HANDLE};
  VmaAllocation m_scene_snapshot_allocation{VK_NULL_HANDLE};
  VkImageView m_scene_snapshot_view{VK_NULL_HANDLE};
  VkImageLayout m_scene_snapshot_layout{VK_IMAGE_LAYOUT_UNDEFINED};

  VkSampler m_depth_sampler{VK_NULL_HANDLE};
  VkSampler m_linear_sampler{VK_NULL_HANDLE};
  eastl::unique_ptr<VulkanTexture> m_noise_texture;

  VkDescriptorSetLayout m_generate_descriptor_layout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_apply_descriptor_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_generate_pipeline_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_apply_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_generate_pipeline{VK_NULL_HANDLE};
  VkPipeline m_apply_pipeline{VK_NULL_HANDLE};

  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_generate_descriptor_set{VK_NULL_HANDLE};
  VkDescriptorSet m_apply_descriptor_set{VK_NULL_HANDLE};

  eastl::unique_ptr<VulkanBuffer> m_uniform_buffer;
};

}  // namespace Blunder
