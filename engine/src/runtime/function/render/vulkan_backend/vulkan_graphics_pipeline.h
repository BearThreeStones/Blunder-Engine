#pragma once

#include "EASTL/array.h"
#include "EASTL/shared_ptr.h"
#include "EASTL/unique_ptr.h"

#include "runtime/function/render/rhi/i_graphics_pipeline.h"

namespace Blunder {

class SlangCompiler;
class VulkanContext;
class VulkanPipeline;

namespace vulkan_backend {

class VulkanCommandList;
class VulkanOffscreenTarget;

class VulkanGraphicsPipeline final : public rhi::IGraphicsPipeline {
 public:
  VulkanGraphicsPipeline() = default;
  ~VulkanGraphicsPipeline() override;

  void bind(VulkanContext* context, SlangCompiler* compiler);

  void initialize(rhi::IOffscreenRenderTarget& render_target,
                  const rhi::GraphicsPipelineDesc& desc) override;
  void shutdown() override;

  rhi::ICommandList* commandList(uint32_t frame_index) override;
  const rhi::ICommandList* commandList(uint32_t frame_index) const override;

  VulkanPipeline* nativePipeline() const { return m_pipeline.get(); }

 private:
  VulkanContext* m_context{nullptr};
  SlangCompiler* m_compiler{nullptr};
  eastl::shared_ptr<VulkanPipeline> m_pipeline;
  static constexpr uint32_t k_max_command_lists = 2;
  eastl::array<eastl::unique_ptr<VulkanCommandList>, k_max_command_lists>
      m_command_lists{};
};

}  // namespace vulkan_backend
}  // namespace Blunder
