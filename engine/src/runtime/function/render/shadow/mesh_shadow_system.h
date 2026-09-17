#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/function/render/gpu_driven/gpu_driven_types.h"
#include "runtime/function/render/shadow/mesh_shadow_casters.h"
#include "runtime/function/render/shadow/virtual_shadow_map.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"

namespace Blunder {

class GpuDrivenRenderer;
class GpuMesh;
class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class VulkanPipeline;
struct ForwardFrameState;
struct ForwardMeshUniformData;
struct ForwardOpaqueDraw;
struct GpuDrivenDraw;
struct ShadowSamplingUniform;

class MeshShadowSystem final {
 public:
  MeshShadowSystem() = default;
  ~MeshShadowSystem();

  void initialize(VulkanContext* context, VulkanAllocator* allocator,
                  SlangCompiler* compiler, bool full = true);
  void shutdown();

  void beginFrame(const ForwardFrameState& frame_state,
                  const GpuDrivenDraw* gpu_draws, uint32_t gpu_draw_count,
                  uint32_t frame_index);

  void recordPageMark(VkCommandBuffer cmd, VkImageView depth_view,
                      VkImage depth_image, uint32_t width, uint32_t height,
                      const glm::mat4& inv_view_projection, uint32_t frame_index);

  void recordFill(VkCommandBuffer cmd, const ForwardFrameState& frame_state,
                  const ForwardOpaqueDraw* opaque_draws, uint32_t opaque_draw_count,
                  uint32_t frame_index);

  void writeSamplingDescriptors(VkDevice device, VkDescriptorSet set,
                                uint32_t first_binding) const;
  void applySamplingUniforms(ShadowSamplingUniform& sampling) const;

  bool meshShadersEnabled() const { return m_mesh_shaders; }
  bool shaderOutputLayerEnabled() const { return m_shader_output_layer; }
  bool vsmEnabled() const { return m_vsm_enabled; }
  const LocalShadowCasters& casters() const { return m_local; }
  uint32_t opaqueCasterMeshlets() const { return m_meshlet_count; }
  uint32_t markedPageCount() const { return m_marked_count; }
  uint32_t vsmOverflow() const { return m_overflow; }
  VkImageView vsmPageView() const { return m_vsm.array_view; }
  VkImageView pointCubeView() const { return m_points.cube_view; }
  VkImageView spotArrayView() const { return m_spots.array_view; }
  VulkanBuffer* pageTableBuffer() const { return m_page_table.get(); }

 private:
  static constexpr uint32_t k_frames = VulkanSync::k_max_frames_in_flight;

  struct DepthArray {
    VkImage image{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkImageView array_view{VK_NULL_HANDLE};
    VkImageView cube_view{VK_NULL_HANDLE};
    eastl::vector<VkImageView> layer_views;
    VkFramebuffer layered_fb{VK_NULL_HANDLE};
    eastl::vector<VkFramebuffer> layer_fbs;
    eastl::vector<VkImageView> slot_views;
    eastl::vector<VkFramebuffer> slot_fbs;
    uint32_t width{1};
    uint32_t height{1};
    uint32_t layers{1};
    bool cube{false};
  };

  struct ShadowMeshUniformCpu {
    glm::mat4 view_projection{1.0f};
    glm::mat4 cube_face_vp[6]{};
    glm::vec4 light_position_range{0.0f};
    uint32_t mode{0};
    uint32_t layer{0};
    uint32_t face_mask{0};
    uint32_t meshlet_count{0};
    glm::uvec4 link_count_pad{0};
    uint32_t link_ids[16]{};
  };

  void createDepthPass();
  void destroyDepthPass();
  void createDepthArray(DepthArray& target, uint32_t width, uint32_t height,
                        uint32_t layers, bool cube);
  void destroyDepthArray(DepthArray& target);
  void createPipelines();
  void destroyPipelines();
  void createBuffers();
  void destroyBuffers();
  void createDescriptors();
  void destroyDescriptors();
  void uploadCasters(const GpuDrivenDraw* draws, uint32_t count,
                     bool scene_static);
  void stampCameraPages();
  void mergeGpuMarksFromStaging(uint32_t frame);
  void compactCpuFlags();
  void barrierDepthArrayToShaderRead(VkCommandBuffer cmd, const DepthArray& target,
                                     uint32_t layers);
  void logVsmFrame();
  void recordMeshTarget(VkCommandBuffer cmd, VkFramebuffer fb, VkExtent2D extent,
                        const ShadowMeshUniformCpu& ubo, uint32_t layer_override,
                        bool layered_pipeline, uint32_t dispatch_mul);
  void recordVsTarget(VkCommandBuffer cmd, VkFramebuffer fb, VkExtent2D extent,
                      const glm::mat4& view_proj,
                      const ForwardOpaqueDraw* opaque_draws,
                      uint32_t opaque_draw_count, uint32_t frame_index);
  void fillLinkIds(const LightComponent* light, ShadowMeshUniformCpu& ubo) const;
  void writeInstanceMeshSets();
  void uploadUboSlot(uint32_t slot, const void* data, VkDeviceSize size);
  void logDrops();

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  SlangCompiler* m_compiler{nullptr};
  bool m_mesh_shaders{false};
  bool m_shader_output_layer{false};
  bool m_vsm_enabled{false};

  VkRenderPass m_depth_pass{VK_NULL_HANDLE};
  VkSampler m_comparison_sampler{VK_NULL_HANDLE};
  VkSampler m_depth_sampler{VK_NULL_HANDLE};

  DepthArray m_vsm{};
  DepthArray m_points{};
  DepthArray m_spots{};
  DepthArray m_dummy{};

  eastl::unique_ptr<VulkanBuffer> m_page_table;
  eastl::unique_ptr<VulkanBuffer> m_page_flags_gpu[k_frames];
  eastl::unique_ptr<VulkanBuffer> m_page_flags_staging[k_frames];
  eastl::unique_ptr<VulkanBuffer> m_instances;
  eastl::unique_ptr<VulkanBuffer> m_meshlets;
  eastl::unique_ptr<VulkanBuffer> m_shadow_ubo;
  eastl::unique_ptr<VulkanBuffer> m_mark_ubos[k_frames];

  VkDescriptorSetLayout m_mesh_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_mesh_pipe_layout{VK_NULL_HANDLE};
  VkPipeline m_mesh_pipeline{VK_NULL_HANDLE};
  VkPipeline m_mesh_layered_pipeline{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_mark_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_mark_pipe_layout{VK_NULL_HANDLE};
  VkPipeline m_mark_pipeline{VK_NULL_HANDLE};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  eastl::vector<VkDescriptorSet> m_mesh_sets;
  VkDescriptorSet m_mark_sets[k_frames]{};
  VkDescriptorSet m_vs_set{VK_NULL_HANDLE};
  uint32_t m_ubo_cursor{0};
  uint32_t m_ubo_stride{256};
  uint32_t m_record_frame{0};
  uint32_t m_physical_capacity{k_vsm_physical_pages};

  eastl::unique_ptr<VulkanPipeline> m_vs_pipeline;

  LocalShadowCasters m_local{};
  glm::mat4 m_vsm_light_view{1.0f};
  glm::vec3 m_camera{0.0f};
  glm::vec3 m_light_dir{0.45f, 0.7f, 0.55f};
  glm::mat4 m_spot_vp[k_max_spot_shadow_maps]{};
  glm::mat4 m_inv_view_projection{1.0f};
  eastl::vector<MeshShadowCasterDraw> m_casters;
  eastl::vector<GpuMesh*> m_instance_meshes;
  eastl::vector<GpuDrivenInstanceGpu> m_instance_cpu;
  eastl::vector<GpuDrivenMeshletGpu> m_meshlet_cpu;
  eastl::vector<uint32_t> m_flags_cpu;
  eastl::vector<uint32_t> m_stamp_virt;
  eastl::vector<uint32_t> m_page_table_cpu;
  eastl::vector<uint32_t> m_physical_to_virtual;
  uint32_t m_instance_count{0};
  uint32_t m_meshlet_count{0};
  uint32_t m_marked_count{0};
  uint32_t m_overflow{0};
  uint32_t m_gpu_mark_count{0};
  uint32_t m_debug_frame{0};
  uint32_t m_last_caster_draw_count{0};
  bool m_casters_ready{false};
  bool m_mesh_sets_written[k_frames]{};
  bool m_logged_point_drop{false};
  bool m_logged_spot_drop{false};
  bool m_logged_page_overflow{false};
  bool m_logged_ubo_overflow{false};
};

}  // namespace Blunder
