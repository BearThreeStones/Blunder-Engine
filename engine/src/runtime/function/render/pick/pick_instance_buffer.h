#pragma once

#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"
#include "runtime/function/render/overlay/pick_overlay.h"
#include "runtime/function/render/pick/pick_instance.h"

#include <vulkan/vulkan.h>

namespace Blunder {

class RenderSystem;
class SceneInstance;
class VulkanAllocator;
class VulkanBuffer;

class PickInstanceBuffer final {
 public:
  PickInstanceBuffer() = default;
  ~PickInstanceBuffer();

  void rebuild(SceneInstance& scene, RenderSystem* render_system);
  void uploadToGpu(VulkanAllocator* alloc);
  void shutdownGpu();

  const eastl::vector<PickOverlay::PickDraw>& pickDraws() const { return m_draws; }
  const eastl::vector<PickInstanceGpu>& gpuInstances() const { return m_instances; }
  uint32_t generation() const { return m_generation; }
  VkBuffer gpuBuffer() const;
  uint32_t gpuInstanceCount() const { return static_cast<uint32_t>(m_instances.size()); }

 private:
  eastl::vector<PickOverlay::PickDraw> m_draws;
  eastl::vector<PickInstanceGpu> m_instances;
  eastl::unique_ptr<VulkanBuffer> m_gpu_buffer;
  uint32_t m_generation{0};
};

}  // namespace Blunder
