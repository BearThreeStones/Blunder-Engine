#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "EASTL/array.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/function/render/clustered/froxel_grid.h"
#include "runtime/function/render/forward/forward_render_path.h"
#include "runtime/function/render/forward/forward_shading.h"
#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/shadow/virtual_shadow_map.h"
#include "runtime/function/render/vulkan/secondary_command_buffer_pool.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/scene/light_eval.h"

namespace Blunder {

class GpuDrivenRenderer;
class MeshShadowSystem;
class ShadowMapTarget;
class SlangCompiler;
class VulkanAllocator;
class VulkanBuffer;
class VulkanContext;
class VulkanTexture;

namespace vulkan_backend {
class VulkanGraphicsPipeline;
}

struct ForwardFrameState;
struct ForwardOpaqueDraw;
struct GpuDrivenDraw;

struct DeferredRenderPathInit {
  VulkanContext* vk_context{nullptr};
  VulkanAllocator* vk_allocator{nullptr};
  SlangCompiler* slang_compiler{nullptr};
  OffscreenRenderTarget* offscreen{nullptr};
  /// Records the shared shadow pass and the post-lighting scene overlay +
  /// transparent secondaries (same UBO ring and Bindless gather as Forward).
  ForwardRenderPath* forward_path{nullptr};
  ShadowMapTarget* shadow_map{nullptr};
  VulkanTexture* fallback_texture{nullptr};
  MeshShadowSystem* mesh_shadows{nullptr};
};

/// G-buffer mesh UBO shared with engine/shaders/gbuffer.slang /
/// gbuffer_skinned.slang (std140).
struct GBufferMeshUniformData {
  glm::mat4 model{1.0f};
  glm::mat4 view{1.0f};
  glm::mat4 projection{1.0f};
  glm::mat4 normal_matrix{1.0f};
  glm::vec4 base_color_factor{1.0f};
  glm::vec4 material_flags{0.0f};
  glm::vec4 metallic_roughness_factors{1.0f, 1.0f, 0.5f, 0.0f};
  glm::vec4 pbr_texture_flags{0.0f};
  glm::uvec4 bindless_texture_indices{0};
  glm::uvec4 receiver{0};
};

/// Lighting UBO shared with engine/shaders/deferred_lighting.slang (std140).
/// Per-receiver light masks live in a storage buffer (14-bit MeshRenderer
/// slots, 16384 entries) so GPU-driven static is not capped at 256.
struct DeferredLightingUniformData {
  glm::mat4 inv_view_projection{1.0f};
  glm::mat4 light_view_projection{1.0f};
  glm::vec4 camera_position{0.0f};
  glm::vec4 shadow_params{0.0f};
  glm::vec4 ambient_color{0.0f};
  glm::vec4 background_color{0.0f};
  glm::vec4 light_count{0.0f};
  glm::mat4 view{1.0f};
  glm::vec4 froxel_screen{1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 froxel_z{0.1f, 1000.0f, 32.0f, 64.0f};
  glm::vec4 clustered_params{0.0f};
  GpuSceneLight lights[k_max_deferred_light_list];
  ShadowSamplingUniform shadow_sampling{};
};

/// Compute UBO shared with engine/shaders/froxel_fill.slang (std140).
struct FroxelFillUniformData {
  glm::mat4 view{1.0f};
  glm::mat4 inv_projection{1.0f};
  glm::vec4 screen{1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 z_params{0.1f, 1000.0f, 32.0f, 64.0f};
  glm::vec4 light_count{0.0f};
};

static_assert(k_max_deferred_light_list <= 32,
              "receiver light masks store one uint32 bit per list entry");
static_assert(ForwardRenderPath::k_max_opaque_draws <= 256);
static_assert(ForwardRenderPath::k_max_opaque_draws % 4 == 0);

/// Deferred Render Path: opaque static + skinned draws write a G-buffer (three
/// path-owned planes sharing the viewport offscreen depth), a fullscreen
/// lighting pass writes the offscreen color, then scene overlays and
/// blend-transparent draws run Forward inside an offscreen LOAD pass. Editor
/// viewport only (`BLUNDER_EDITOR_DEFERRED`); previews and the Player stay on
/// `ForwardRenderPath`. Decision: docs/adr/0062-deferred-render-path.md.
class DeferredRenderPath final {
 public:
  static constexpr uint32_t k_max_receiver_slots =
      ForwardRenderPath::k_max_opaque_draws;
  static constexpr uint32_t k_gbuffer_plane_count = 3;
  /// R16_UINT receiver clear: no opaque geometry wrote this pixel.
  static constexpr uint32_t k_receiver_no_geometry = 0xFFFFu;
  static constexpr VkFormat k_albedo_ao_format = VK_FORMAT_R8G8B8A8_UNORM;
  static constexpr VkFormat k_normal_metal_rough_format =
      VK_FORMAT_R8G8B8A8_UNORM;
  /// Preferred receiver format. Runtime may fall back to R32_UINT.
  static constexpr VkFormat k_receiver_format = VK_FORMAT_R16_UINT;

  DeferredRenderPath() = default;
  ~DeferredRenderPath();

  void initialize(const DeferredRenderPathInit& init);
  void shutdown();

  /// Recreates the G-buffer planes and framebuffers for the offscreen extent.
  /// Must run after `OffscreenRenderTarget::resize` (framebuffers reference
  /// the offscreen depth and color views).
  void resize(uint32_t width, uint32_t height);

  /// Destroys G-buffer framebuffers and planes so the offscreen color/depth
  /// views they reference can be destroyed. Call after `vkDeviceWaitIdle`
  /// and before `OffscreenRenderTarget::resize`.
  void dropGpuTargets();

  /// Records the Directional shadow pass then the G-buffer render pass.
  /// Frame graph G-buffer Pass callback (ADR 0069). Barriers stay on the
  /// PRIMARY (ADR 0059).
  void recordGBufferPass(VkCommandBuffer command_buffer,
                         const ForwardFrameState& frame_state,
                         const ForwardOpaqueDraw* opaque_draws,
                         uint32_t opaque_draw_count, uint32_t frame_index,
                         GpuDrivenRenderer* gpu_driven = nullptr,
                         const GpuDrivenDraw* gpu_draws = nullptr,
                         uint32_t gpu_draw_count = 0);

  /// Records lighting into the offscreen color, then LOAD scene overlay +
  /// transparent. Frame graph Lighting Pass callback (ADR 0069).
  void recordLightingPass(VkCommandBuffer command_buffer,
                          const ForwardFrameState& frame_state,
                          const ForwardOpaqueDraw* opaque_draws,
                          uint32_t opaque_draw_count,
                          const ForwardOpaqueDraw* transparent_draws,
                          uint32_t transparent_draw_count, uint32_t frame_index,
                          GpuDrivenRenderer* gpu_driven = nullptr);

  VkRenderPass gbufferRenderPass() const { return m_gbuffer_render_pass; }
  VkRenderPass lightingRenderPass() const { return m_lighting_render_pass; }

  /// Last completed FIF's per-frame froxel overflow (dropped assignments).
  uint32_t froxelDroppedLightAssignments() const {
    return m_froxel_dropped_light_assignments;
  }
  uint64_t froxelDroppedLightAssignmentsTotal() const {
    return m_froxel_dropped_light_assignments_total;
  }

 private:
  struct GBufferSlot {
    VkImage images[k_gbuffer_plane_count]{};
    VmaAllocation allocations[k_gbuffer_plane_count]{};
    VkImageView views[k_gbuffer_plane_count]{};
    VkFramebuffer gbuffer_framebuffer{VK_NULL_HANDLE};
    VkFramebuffer lighting_framebuffer{VK_NULL_HANDLE};
  };

  void createRenderPasses();
  void destroyRenderPasses();
  void createPipelines();
  void destroyPipelines();
  void createDescriptorResources();
  void destroyDescriptorResources();
  void createSlot(uint32_t slot_index);
  void destroySlot(uint32_t slot_index);
  void destroySlots();

  bool prepareViewportRecord(uint32_t frame_index, VkExtent2D* extent,
                             uint32_t* slot_index) const;
  void drawGBufferList(VkCommandBuffer cmd, const ForwardFrameState& frame_state,
                       const ForwardOpaqueDraw* opaque_draws,
                       uint32_t opaque_draw_count, uint32_t frame_index);
  void uploadLightingUniforms(const ForwardFrameState& frame_state,
                              const ForwardOpaqueDraw* opaque_draws,
                              uint32_t opaque_draw_count, uint32_t frame_index,
                              GpuDrivenRenderer* gpu_driven);
  void writeLightingDescriptors(uint32_t frame_index, uint32_t slot_index);
  void writeFroxelDescriptors(uint32_t frame_index);
  void createFroxelFillPipeline();
  void destroyFroxelFillPipeline();
  void recreateFroxelGridBuffers(uint32_t width, uint32_t height);
  void recordFroxelFill(VkCommandBuffer cmd, const ForwardFrameState& frame_state,
                        uint32_t frame_index);
  void cmdBarrierForLoadPass(VkCommandBuffer cmd);

  VulkanContext* m_vk_context{nullptr};
  VulkanAllocator* m_vk_allocator{nullptr};
  SlangCompiler* m_slang_compiler{nullptr};
  OffscreenRenderTarget* m_offscreen{nullptr};
  ForwardRenderPath* m_forward_path{nullptr};
  ShadowMapTarget* m_shadow_map{nullptr};
  VulkanTexture* m_fallback_texture{nullptr};
  MeshShadowSystem* m_mesh_shadows{nullptr};

  uint32_t m_width{0};
  uint32_t m_height{0};
  VkFormat m_receiver_format{k_receiver_format};

  VkRenderPass m_gbuffer_render_pass{VK_NULL_HANDLE};
  VkRenderPass m_lighting_render_pass{VK_NULL_HANDLE};
  eastl::array<GBufferSlot, OffscreenRenderTarget::k_buffer_count> m_slots{};

  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_gbuffer_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline>
      m_skinned_gbuffer_pipeline;
  eastl::unique_ptr<vulkan_backend::VulkanGraphicsPipeline> m_lighting_pipeline;

  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_gbuffer_uniform_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_skinned_gbuffer_uniform_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_skinned_bone_palette_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_lighting_uniform_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_receiver_mask_buffers;
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  eastl::vector<VkDescriptorSet> m_gbuffer_descriptor_sets;
  eastl::vector<VkDescriptorSet> m_skinned_gbuffer_descriptor_sets;
  eastl::array<VkDescriptorSet, VulkanSync::k_max_frames_in_flight>
      m_lighting_descriptor_sets{};

  VkDescriptorSetLayout m_froxel_fill_set_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_froxel_fill_pipe_layout{VK_NULL_HANDLE};
  VkPipeline m_froxel_fill_pipeline{VK_NULL_HANDLE};
  eastl::array<VkDescriptorSet, VulkanSync::k_max_frames_in_flight>
      m_froxel_fill_descriptor_sets{};
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_froxel_fill_ubos;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_clustered_light_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_froxel_index_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_froxel_count_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_froxel_overflow_buffers;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_froxel_overflow_readbacks;
  eastl::vector<eastl::unique_ptr<VulkanBuffer>> m_clustered_mask_buffers;
  FroxelGridDim m_froxel_dim{};
  uint32_t m_uploaded_clustered_light_count{0};
  uint32_t m_froxel_dropped_light_assignments{0};
  uint64_t m_froxel_dropped_light_assignments_total{0};

  eastl::vector<uint32_t> m_receiver_mask_cpu;
  eastl::vector<uint32_t> m_clustered_mask_cpu;
  GpuSceneLight m_cached_clustered_gpu[k_max_clustered_lights]{};
  uint64_t m_lighting_mask_fingerprint{0};
  uint64_t m_uploaded_lighting_mask_fingerprint[VulkanSync::k_max_frames_in_flight]{};
  eastl::array<uint32_t, VulkanSync::k_max_frames_in_flight> m_lighting_desc_slot{
      ~0u, ~0u};
  eastl::array<uint8_t, VulkanSync::k_max_frames_in_flight> m_froxel_desc_written{};
};

}  // namespace Blunder
