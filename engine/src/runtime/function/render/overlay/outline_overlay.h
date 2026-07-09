#pragma once

#include <glm/mat4x4.hpp>

#include "EASTL/vector.h"

#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/overlay/overlay_base.h"

namespace Blunder {

class OffscreenRenderTarget;
class OutlineTargets;
class SlangCompiler;
class VulkanAllocator;
class VulkanContext;

/// Two-pass selection outline: ID prepass + edge resolve composite.
class OutlineOverlay final : public Overlay {
 public:
  OutlineOverlay() = default;
  ~OutlineOverlay();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                  OffscreenRenderTarget* offscreen, SlangCompiler* compiler,
                  OutlineTargets* targets);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  void begin_sync(OverlayResources& res, const OverlayState& state) override;

  void drawPrepass(VkCommandBuffer cmd, const OverlayState& state);
  void drawResolve(VkCommandBuffer cmd, OffscreenRenderTarget* offscreen,
                   const OverlayState& state);

 private:
  struct CachedDraw {
    GpuMesh* gpu_mesh{nullptr};
    glm::mat4 world_matrix{1.0f};
    bool double_sided{false};
    uint16_t packed_id{0};
  };

  void createPrepassPipeline();
  void destroyPrepassPipeline();
  void createResolvePipeline();
  void destroyResolvePipeline();
  void createResolveRenderPass();
  void destroyResolveRenderPass();
  void createDescriptorResources();
  void destroyDescriptorResources();
  void writeResolveDescriptors(OffscreenRenderTarget* offscreen);

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  SlangCompiler* m_compiler{nullptr};
  OffscreenRenderTarget* m_offscreen{nullptr};
  OutlineTargets* m_targets{nullptr};

  uint32_t m_width{0};
  uint32_t m_height{0};

  eastl::vector<CachedDraw> m_cached_draws;

  VkSampler m_depth_sampler{VK_NULL_HANDLE};

  VkDescriptorSetLayout m_prepass_descriptor_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_prepass_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_prepass_pipeline{VK_NULL_HANDLE};
  VkDescriptorPool m_prepass_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_prepass_descriptor_set{VK_NULL_HANDLE};
  eastl::unique_ptr<class VulkanBuffer> m_prepass_uniform_buffer;

  VkRenderPass m_resolve_render_pass{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_resolve_descriptor_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_resolve_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_resolve_pipeline{VK_NULL_HANDLE};
  VkDescriptorPool m_resolve_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_resolve_descriptor_set{VK_NULL_HANDLE};
  eastl::unique_ptr<class VulkanBuffer> m_resolve_uniform_buffer;
};

}  // namespace Blunder
