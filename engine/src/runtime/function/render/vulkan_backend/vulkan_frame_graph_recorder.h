#pragma once

#include <vulkan/vulkan.h>

#include "runtime/function/render/frame_graph/frame_graph.h"

namespace Blunder {
namespace vulkan_backend {

struct FrameGraphVulkanMapping {
  VkPipelineStageFlags src_stage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
  VkPipelineStageFlags dst_stage{VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
  VkAccessFlags src_access{0};
  VkAccessFlags dst_access{0};
  VkImageLayout old_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout new_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  bool is_buffer{false};
};

FrameGraphVulkanMapping mapFrameGraphBarrier(
    const FrameGraphBarrier& barrier, FrameGraphResourceShape shape);

class VulkanFrameGraphRecorder final : public IFrameGraphRecorder {
 public:
  void bind(FrameGraph* graph, VkCommandBuffer command_buffer);

  void pipelineBarrier(const FrameGraphBarrier& barrier) override;

 private:
  FrameGraph* m_graph{nullptr};
  VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
};

}  // namespace vulkan_backend
}  // namespace Blunder
