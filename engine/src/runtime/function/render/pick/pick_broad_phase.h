#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/vector.h"

#include "EASTL/unique_ptr.h"
#include "runtime/core/math/geometry.h"
#include "runtime/function/render/pick/pick_broad_hits.h"

namespace Blunder {

class SlangCompiler;
class VulkanAllocator;
class VulkanContext;

/// GPU compute broad-phase ray vs pick-instance AABB.
class PickBroadPhase final {
 public:
  static constexpr uint32_t k_max_hits = 1024;

  PickBroadPhase() = default;
  ~PickBroadPhase();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc, SlangCompiler* compiler);
  void shutdown();

  void recordDispatch(VkCommandBuffer cmd, const Ray& ray, VkBuffer instance_buffer,
                      uint32_t instance_count);

  void readbackHits(eastl::vector<BroadHit>& out);

 private:
  struct BroadHitGpuRecord {
    uint32_t entity_id{0};
    float t{0.0f};
  };

  struct PickBroadUniforms {
    glm::vec3 ray_origin{0.0f};
    float ray_pad0{0.0f};
    glm::vec3 ray_dir{0.0f, 0.0f, 1.0f};
    float ray_pad1{0.0f};
    uint32_t instance_count{0};
    uint32_t max_hits{k_max_hits};
    uint32_t pad0{0};
    uint32_t pad1{0};
  };

  void createPipeline();
  void destroyPipeline();
  void createDescriptorResources();
  void destroyDescriptorResources();
  void updateInstanceBufferDescriptor(VkBuffer instance_buffer);

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  SlangCompiler* m_compiler{nullptr};

  eastl::unique_ptr<class VulkanBuffer> m_uniform_buffer;
  eastl::unique_ptr<class VulkanBuffer> m_hit_records_buffer;
  eastl::unique_ptr<class VulkanBuffer> m_hit_count_buffer;

  VkDescriptorSetLayout m_descriptor_layout{VK_NULL_HANDLE};
  VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
  VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
};

}  // namespace Blunder
