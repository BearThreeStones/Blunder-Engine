#pragma once

#include <cstdint>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"

#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/volumetric_fog_math.h"

namespace Blunder {

class OffscreenRenderTarget;
class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;

/// Viewport / Player volumetric fog: density / scatter / temporal / integrate /
/// composite on the presented color (`color * T + inscatter`). Same pass for
/// editor Deferred lighting and Player Forward. Hand-written layouts; 3D volumes
/// stay off the Bindless color table.
class VolumetricFogPass final {
 public:
  VolumetricFogPass() = default;
  ~VolumetricFogPass();

  void initialize(VulkanContext* context, VulkanAllocator* allocator,
                  SlangCompiler* compiler);
  void shutdown();

  void apply(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen,
             const ForwardFrameState& frame_state, const ActiveFog& fog,
             uint32_t frame_index);

  /// Drop scatter history so a new scene does not reproject the previous volume.
  void invalidateHistory();

 private:
  struct Volume3D {
    VkImage image{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
  };

  void createPipelines();
  void destroyPipelines();
  void createDescriptorResources();
  void destroyDescriptorResources();
  void createCompositePass();
  void destroyCompositePass();
  void ensureVolumes(uint32_t width, uint32_t height);
  void destroyVolumes();
  void barrierVolume(VkCommandBuffer cmd, Volume3D& volume,
                     VkImageLayout new_layout, VkAccessFlags src_access,
                     VkAccessFlags dst_access, VkPipelineStageFlags src_stage,
                     VkPipelineStageFlags dst_stage);
  void writeDensityDescriptors(uint32_t slot);
  void writeScatterDescriptors(uint32_t slot, uint32_t history_index,
                               uint32_t current_index);
  void writeIntegrateDescriptors(uint32_t slot, uint32_t scatter_index);
  void writeCompositeDescriptors(uint32_t slot, OffscreenRenderTarget* offscreen);
  void copySceneSnapshot(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen,
                         uint32_t slot);
  void dispatchCompute(VkCommandBuffer cmd, VkPipeline pipeline,
                       VkPipelineLayout layout, VkDescriptorSet set,
                       uint32_t groups_x, uint32_t groups_y, uint32_t groups_z);
  void invalidateSlotCaches();

  static constexpr uint32_t k_fog_frames = 2;

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  SlangCompiler* m_compiler{nullptr};

  uint32_t m_view_width{0};
  uint32_t m_view_height{0};
  FroxelGridSize m_grid{};

  Volume3D m_density[k_fog_frames]{};
  Volume3D m_scatter[2]{};
  Volume3D m_integrated[k_fog_frames]{};

  VkImage m_scene_snapshot_image[k_fog_frames]{};
  VmaAllocation m_scene_snapshot_allocation[k_fog_frames]{};
  VkImageView m_scene_snapshot_view[k_fog_frames]{};
  VkImageLayout m_scene_snapshot_layout[k_fog_frames]{};

  VkSampler m_linear_sampler{VK_NULL_HANDLE};
  VkSampler m_depth_sampler{VK_NULL_HANDLE};

  VkDescriptorSetLayout m_density_layout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_scatter_layout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_integrate_layout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_composite_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_density_pipeline_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_scatter_pipeline_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_integrate_pipeline_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_composite_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_density_pipeline{VK_NULL_HANDLE};
  VkPipeline m_scatter_pipeline{VK_NULL_HANDLE};
  VkPipeline m_integrate_pipeline{VK_NULL_HANDLE};
  VkPipeline m_composite_pipeline{VK_NULL_HANDLE};

  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_density_set[k_fog_frames]{};
  VkDescriptorSet m_scatter_set[k_fog_frames]{};
  VkDescriptorSet m_integrate_set[k_fog_frames]{};
  VkDescriptorSet m_composite_set[k_fog_frames]{};

  VkRenderPass m_composite_render_pass{VK_NULL_HANDLE};
  VkFramebuffer m_composite_framebuffer[k_fog_frames]{};
  VkImageView m_composite_color_view[k_fog_frames]{};
  VkImageView m_composite_depth_view[k_fog_frames]{};
  bool m_density_desc_valid[k_fog_frames]{};
  bool m_composite_desc_valid[k_fog_frames]{};
  uint32_t m_scatter_desc_history[k_fog_frames]{~0u, ~0u};
  uint32_t m_scatter_desc_current[k_fog_frames]{~0u, ~0u};
  uint32_t m_integrate_desc_scatter[k_fog_frames]{~0u, ~0u};

  eastl::unique_ptr<VulkanBuffer> m_uniform_buffer[k_fog_frames];

  glm::mat4 m_prev_view_proj{1.0f};
  glm::vec3 m_prev_view_forward{0.0f, 0.0f, -1.0f};
  bool m_history_valid{false};
  uint32_t m_history_index{0};
};

}  // namespace Blunder
