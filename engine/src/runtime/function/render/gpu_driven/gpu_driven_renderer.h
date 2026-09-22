#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/function/render/gpu_driven/gpu_driven_types.h"
#include "runtime/function/render/slang/shader_resource_layout.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder {

class GpuMesh;
class MeshShadowSystem;
class SlangCompiler;
class ShadowMapTarget;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class VulkanPipeline;
class VulkanTexture;
struct ForwardFrameState;

class GpuDrivenRenderer final {
 public:
  GpuDrivenRenderer() = default;
  ~GpuDrivenRenderer();

  void initialize(VulkanContext* context, VulkanAllocator* allocator,
                  SlangCompiler* compiler, VkRenderPass offscreen_pass,
                  VkRenderPass shadow_pass, VkRenderPass gbuffer_pass);
  void setMeshShadows(MeshShadowSystem* mesh_shadows) {
    m_mesh_shadows = mesh_shadows;
  }
  void shutdown();
  void resizeHiZ(uint32_t width, uint32_t height);
  /// Drop previous-scene HiZ so the first record of a new document does not
  /// occlusion-cull courtyard meshlets against the old depth pyramid.
  void invalidateSceneOcclusion();

  void uploadAndCull(VkCommandBuffer cmd, uint32_t frame,
                     const GpuDrivenDraw* draws, uint32_t count,
                     const ForwardFrameState& frame_state, bool enable_hiz);
  void recordShadowCull(VkCommandBuffer cmd, uint32_t frame,
                        const ForwardFrameState& frame_state);

  void recordOpaqueIndirect(VkCommandBuffer cmd, uint32_t frame,
                            const ForwardFrameState& frame_state, bool late,
                            ShadowMapTarget* shadow, VulkanTexture* fallback);
  void recordGBufferIndirect(VkCommandBuffer cmd, uint32_t frame,
                             const ForwardFrameState& frame_state, bool late,
                             VulkanTexture* fallback);
  void recordShadowIndirect(VkCommandBuffer cmd, uint32_t frame,
                            const ForwardFrameState& frame_state);
  void recordBuildHiZ(VkCommandBuffer cmd, uint32_t frame, VkImageView depth_view,
                      uint32_t width, uint32_t height,
                      VkImage depth_image = VK_NULL_HANDLE);

  bool meshShadersEnabled() const { return m_mesh_shaders_enabled; }
  bool latePassEnabled() const;
  uint32_t instanceCount() const { return m_instance_count; }
  uint32_t batchCount() const {
    return static_cast<uint32_t>(m_batches.size());
  }
  /// Early + late compact surviving meshlet indirect commands (last harvested
  /// in-flight slot). Not a vkCmd* count.
  uint32_t survivingIndirectCount() const {
    return m_surviving_early + m_surviving_late;
  }
  void harvestHudCounts(uint32_t frame);
  const eastl::vector<GpuDrivenDraw>& packedDraws() const {
    return m_packed_draws;
  }

 private:
  static constexpr uint32_t k_frames = VulkanSync::k_max_frames_in_flight;
  static constexpr uint32_t k_max_hiz_mips = 12u;
  static constexpr uint32_t k_max_mesh_batches = 512u;

  struct MeshBatch {
    GpuMesh* mesh{nullptr};
    uint32_t meshlet_first{0};
    uint32_t meshlet_count{0};
  };

  struct FrameBuffers {
    eastl::unique_ptr<VulkanBuffer> instances;
    eastl::unique_ptr<VulkanBuffer> meshlets;
    eastl::unique_ptr<VulkanBuffer> early_cmds;
    eastl::unique_ptr<VulkanBuffer> late_cmds;
    eastl::unique_ptr<VulkanBuffer> shadow_cmds;
    eastl::unique_ptr<VulkanBuffer> dummy_cmds;
    eastl::unique_ptr<VulkanBuffer> early_counts;
    eastl::unique_ptr<VulkanBuffer> late_counts;
    eastl::unique_ptr<VulkanBuffer> shadow_counts;
    eastl::unique_ptr<VulkanBuffer> dummy_counts;
    eastl::unique_ptr<VulkanBuffer> early_count_readback;
    eastl::unique_ptr<VulkanBuffer> late_count_readback;
    eastl::unique_ptr<VulkanBuffer> batch_bases;
    eastl::unique_ptr<VulkanBuffer> cull_ubo;
    eastl::unique_ptr<VulkanBuffer> shadow_cull_ubo;
    eastl::unique_ptr<VulkanBuffer> view_ubo;
    eastl::unique_ptr<VulkanBuffer> gbuffer_ubo;
    eastl::unique_ptr<VulkanBuffer> shadow_ubo;
    eastl::unique_ptr<VulkanBuffer> hiz_ubos[k_max_hiz_mips];
  };

  struct HizPyramid {
    VkImage image{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkImageView sampled_view{VK_NULL_HANDLE};
    VkImageView mip_views[k_max_hiz_mips]{};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t mip_count{0};
    bool built{false};
  };

  void createPipelines(VkRenderPass offscreen_pass, VkRenderPass shadow_pass,
                       VkRenderPass gbuffer_pass);
  void destroyPipelines();
  void createBuffers();
  void destroyBuffers();
  void createDescriptors();
  void destroyDescriptors();
  void createComputePipeline(const char* shader_path, const uint32_t* bindings,
                             const uint32_t* sets, uint32_t binding_count,
                             const ShaderDescriptorKind* kinds,
                             VkDescriptorSetLayout* layout, VkPipelineLayout* pipe_layout,
                             VkPipeline* pipeline);
  void createMeshPipeline(VkRenderPass render_pass);
  void destroyHiz();
  void createHiz(uint32_t width, uint32_t height);
  void packDraws(const GpuDrivenDraw* draws, uint32_t count,
                 const ForwardFrameState& frame_state);
  void updateInstanceTransforms(const GpuDrivenDraw* draws, uint32_t count);
  void updateCullDescriptors(uint32_t frame, VkDescriptorSet set, VulkanBuffer* ubo,
                             VkBuffer early_cmds, VkBuffer late_cmds,
                             VkBuffer early_counts, VkBuffer late_counts,
                             VkImageView hiz_view, bool hiz_enabled);
  void dispatchCull(VkCommandBuffer cmd, uint32_t frame, VkDescriptorSet set,
                    VulkanBuffer* ubo, const glm::mat4& view_projection,
                    const glm::vec3& camera, bool hiz_enabled, bool skip_cone,
                    VkBuffer early_cmds, VkBuffer late_cmds, VkBuffer early_counts,
                    VkBuffer late_counts);
  bool compactIndirectEnabled() const;
  void copyHudCounts(VkCommandBuffer cmd, uint32_t frame);
  void recordIndirectBatches(VkCommandBuffer cmd, uint32_t frame, bool late,
                             bool gbuffer, bool shadow, ShadowMapTarget* shadow_map,
                             VulkanTexture* fallback);
  void writePbrDescriptors(uint32_t frame, ShadowMapTarget* shadow,
                           VulkanTexture* fallback);
  void writeGBufferDescriptors(uint32_t frame);
  void writeShadowDescriptors(uint32_t frame);

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  SlangCompiler* m_compiler{nullptr};
  MeshShadowSystem* m_mesh_shadows{nullptr};
  bool m_mesh_shaders_enabled{false};

  eastl::unique_ptr<VulkanPipeline> m_pbr_pipeline;
  eastl::unique_ptr<VulkanPipeline> m_gbuffer_pipeline;
  eastl::unique_ptr<VulkanPipeline> m_shadow_pipeline;

  VkDescriptorSetLayout m_cull_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_cull_pipe_layout{VK_NULL_HANDLE};
  VkPipeline m_cull_pipeline{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_hiz_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_hiz_pipe_layout{VK_NULL_HANDLE};
  VkPipeline m_hiz_pipeline{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_mesh_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_mesh_pipe_layout{VK_NULL_HANDLE};
  VkPipeline m_mesh_pipeline{VK_NULL_HANDLE};

  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_cull_sets[k_frames]{};
  VkDescriptorSet m_shadow_cull_sets[k_frames]{};
  VkDescriptorSet m_hiz_sets[k_frames][k_max_hiz_mips]{};
  VkDescriptorSet m_pbr_sets[k_frames]{};
  VkDescriptorSet m_gbuffer_sets[k_frames]{};
  VkDescriptorSet m_shadow_sets[k_frames]{};
  VkDescriptorSet m_mesh_sets[k_frames][2][k_max_mesh_batches]{};

  FrameBuffers m_frames[k_frames];
  HizPyramid m_hiz[k_frames];
  VkSampler m_hiz_sampler{VK_NULL_HANDLE};
  VkImage m_dummy_hiz_image{VK_NULL_HANDLE};
  VmaAllocation m_dummy_hiz_alloc{VK_NULL_HANDLE};
  VkImageView m_dummy_hiz_view{VK_NULL_HANDLE};

  eastl::vector<GpuDrivenDraw> m_packed_draws;
  eastl::vector<MeshBatch> m_batches;
  eastl::vector<GpuDrivenInstanceGpu> m_instance_cpu;
  eastl::vector<GpuDrivenMeshletGpu> m_meshlet_cpu;
  eastl::vector<uint32_t> m_batch_base_cpu;
  uint32_t m_instance_count{0};
  uint32_t m_meshlet_count{0};
  uint32_t m_surviving_early{0};
  uint32_t m_surviving_late{0};
  uint32_t m_hud_copied_batches[k_frames]{};
  uint32_t m_hiz_width{0};
  uint32_t m_hiz_height{0};
  bool m_late_pass_enabled{false};
  uint64_t m_packed_fingerprint{0};
  uint64_t m_identity_fingerprint{0};
  uint64_t m_transform_fingerprint{0};
  uint64_t m_mask_fingerprint{0};
  uint64_t m_lights_fingerprint{0};
  uint64_t m_uploaded_fingerprint[k_frames]{};
  uint64_t m_uploaded_identity_fingerprint[k_frames]{};
  VkImageView m_hiz_bound_src[k_frames][k_max_hiz_mips]{};
  VkImageView m_hiz_bound_dst[k_frames][k_max_hiz_mips]{};
  VkImageView m_cull_hiz_bound[k_frames]{};
  VkImageView m_shadow_cull_hiz_bound[k_frames]{};
};

}  // namespace Blunder
