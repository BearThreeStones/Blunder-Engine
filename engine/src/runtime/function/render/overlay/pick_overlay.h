#pragma once

#include <cstdint>

#include <cgltf.h>
#include <glm/mat4x4.hpp>

#include "EASTL/vector.h"

#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/overlay/overlay_base.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class EditorCamera;
class PickTargets;
class RenderSystem;
class SceneInstance;
class SlangCompiler;
class VulkanAllocator;
class VulkanContext;

/// On-demand GPU entity-ID pick prepass for viewport selection.
class PickOverlay final : public Overlay {
 public:
  PickOverlay() = default;
  ~PickOverlay();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc, SlangCompiler* compiler,
                    PickTargets* targets);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  void begin_sync(OverlayResources& res, const OverlayState& state) override {}

  EntityId pickAtWindowPosition(float window_x, float window_y, EditorCamera& camera,
                                SceneInstance& scene, RenderSystem& render_system);

  eastl::vector<EntityId> pickAllAtWindowPosition(float window_x, float window_y,
                                                  EditorCamera& camera,
                                                  SceneInstance& scene,
                                                  RenderSystem& render_system);

  struct PickDraw {
    GpuMesh* gpu_mesh{nullptr};
    glm::mat4 world_matrix{1.0f};
    EntityId entity_id{k_invalid_entity_id};
    cgltf_alpha_mode alpha_mode{cgltf_alpha_mode_opaque};
    float alpha_cutoff{0.5f};
    bool has_base_color_texture{false};
    float base_color_alpha{1.0f};
    class VulkanTexture* base_color_texture{nullptr};
  };

  struct PickSample {
    EntityId entity_id{k_invalid_entity_id};
    float depth{k_pick_depth_clear_zo};
  };

  eastl::vector<PickDraw> collectPickableDraws(SceneInstance& scene,
                                               RenderSystem& render_system);

  /// Returns draws whose `entity_id` is not listed in `excluded_entities`.
  static eastl::vector<PickDraw> filterPickDraws(
      const eastl::vector<PickDraw>& draws,
      const eastl::vector<EntityId>& excluded_entities);

  /// Returns draws whose `entity_id` is listed in `include_entities`.
  static eastl::vector<PickDraw> filterPickDrawsToEntities(
      const eastl::vector<PickDraw>& draws,
      const eastl::vector<EntityId>& include_entities);

  void recordPixelPickPass(VkCommandBuffer cmd, int32_t pixel_x, int32_t pixel_y,
                           const eastl::vector<PickDraw>& draws,
                           const glm::mat4& view, const glm::mat4& projection,
                           RenderSystem& render_system);

  void recordPixelPickPass(VkCommandBuffer cmd, int32_t pixel_x, int32_t pixel_y,
                           const eastl::vector<PickDraw>& draws,
                           const glm::mat4& view, const glm::mat4& projection,
                           RenderSystem& render_system, float peel_depth,
                           bool is_peel_pass);

  PickSample readSubmittedPickSample() const;

  bool resolvePickPixel(float window_x, float window_y, EditorCamera& camera,
                        int32_t& out_pixel_x, int32_t& out_pixel_y) const;

  static constexpr int k_max_peel_count = 16;
  static constexpr float k_peel_depth_epsilon = 2e-5f;
  /// Depth clear for pick prepass (matches glm::perspectiveZO + VK_COMPARE_OP_GREATER).
  static constexpr float k_pick_depth_clear_zo = 0.0f;

  static bool isPickSampleMiss(EntityId entity_id, float depth);

 private:
  using CachedDraw = PickDraw;

  PickSample samplePixelPickPass(int32_t pixel_x, int32_t pixel_y,
                                 const eastl::vector<CachedDraw>& draws,
                                 const glm::mat4& view, const glm::mat4& projection,
                                 RenderSystem& render_system);

  PickSample samplePixelPickPass(int32_t pixel_x, int32_t pixel_y,
                                 const eastl::vector<CachedDraw>& draws,
                                 const glm::mat4& view, const glm::mat4& projection,
                                 RenderSystem& render_system, float peel_depth,
                                 bool is_peel_pass);

  void drawPickPrepass(VkCommandBuffer cmd, const eastl::vector<CachedDraw>& draws,
                       const glm::mat4& view, const glm::mat4& projection,
                       RenderSystem& render_system, float peel_depth,
                       bool is_peel_pass);

  void createPipeline();
  void destroyPipeline();
  void createDescriptorResources();
  void destroyDescriptorResources();
  void writeDescriptorTexture(class VulkanTexture* texture);

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  SlangCompiler* m_compiler{nullptr};
  PickTargets* m_targets{nullptr};

  uint32_t m_width{0};
  uint32_t m_height{0};

  VkSampler m_sampler{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descriptor_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};
  eastl::unique_ptr<class VulkanBuffer> m_uniform_buffer;
  eastl::unique_ptr<class VulkanBuffer> m_readback_buffer;
  eastl::unique_ptr<class VulkanBuffer> m_depth_readback_buffer;
};

}  // namespace Blunder
