#include "runtime/function/render/vulkan_backend/vulkan_frame_graph_recorder.h"

#include <cstdio>

#include <vulkan/vulkan.h>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

Blunder::FrameGraphBarrier makeBarrier(Blunder::FrameGraphUsage from_usage,
                                       Blunder::FrameGraphAccessKind from_access,
                                       bool from_undefined,
                                       Blunder::FrameGraphUsage to_usage,
                                       Blunder::FrameGraphAccessKind to_access) {
  Blunder::FrameGraphBarrier barrier{};
  barrier.from.undefined = from_undefined;
  barrier.from.usage = from_usage;
  barrier.from.access = from_access;
  barrier.to.undefined = false;
  barrier.to.usage = to_usage;
  barrier.to.access = to_access;
  return barrier;
}

}  // namespace

int main() {
  using Blunder::FrameGraphAccessKind;
  using Blunder::FrameGraphResourceShape;
  using Blunder::FrameGraphUsage;
  using Blunder::vulkan_backend::mapFrameGraphBarrier;

  const Blunder::FrameGraphBarrier color = makeBarrier(
      FrameGraphUsage::Sampled, FrameGraphAccessKind::Read, true,
      FrameGraphUsage::ColorAttachment, FrameGraphAccessKind::Write);
  const auto color_mapped =
      mapFrameGraphBarrier(color, FrameGraphResourceShape::Texture);
  expect_true("Undefined from is UNDEFINED / TOP_OF_PIPE",
              color_mapped.old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
                  color_mapped.src_access == 0 &&
                  color_mapped.src_stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
  expect_true("ColorAttachment Write is COLOR_ATTACHMENT_OPTIMAL",
              color_mapped.new_layout ==
                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
                  (color_mapped.dst_access &
                   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) != 0 &&
                  (color_mapped.dst_stage &
                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) != 0 &&
                  !color_mapped.is_buffer);

  const Blunder::FrameGraphBarrier depth = makeBarrier(
      FrameGraphUsage::DepthAttachment, FrameGraphAccessKind::Write, false,
      FrameGraphUsage::DepthAttachment, FrameGraphAccessKind::Write);
  const auto depth_mapped =
      mapFrameGraphBarrier(depth, FrameGraphResourceShape::Texture);
  expect_true("DepthAttachment Write is DEPTH_STENCIL_ATTACHMENT_OPTIMAL",
              depth_mapped.new_layout ==
                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
                  (depth_mapped.dst_access &
                   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) != 0 &&
                  !depth_mapped.is_buffer);

  const Blunder::FrameGraphBarrier sampled = makeBarrier(
      FrameGraphUsage::ColorAttachment, FrameGraphAccessKind::Write, false,
      FrameGraphUsage::Sampled, FrameGraphAccessKind::Read);
  const auto sampled_mapped =
      mapFrameGraphBarrier(sampled, FrameGraphResourceShape::Texture);
  expect_true("Sampled Read is SHADER_READ_ONLY_OPTIMAL",
              sampled_mapped.new_layout ==
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                  (sampled_mapped.dst_access & VK_ACCESS_SHADER_READ_BIT) !=
                      0 &&
                  !sampled_mapped.is_buffer);

  const Blunder::FrameGraphBarrier storage = makeBarrier(
      FrameGraphUsage::Storage, FrameGraphAccessKind::Write, false,
      FrameGraphUsage::Storage, FrameGraphAccessKind::Read);
  const auto storage_mapped =
      mapFrameGraphBarrier(storage, FrameGraphResourceShape::Buffer);
  expect_true("Buffer Storage is buffer barrier with shader access",
              storage_mapped.is_buffer &&
                  storage_mapped.old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
                  storage_mapped.new_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
                  (storage_mapped.src_access & VK_ACCESS_SHADER_WRITE_BIT) !=
                      0 &&
                  (storage_mapped.dst_access & VK_ACCESS_SHADER_READ_BIT) != 0);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d FAIL\n", g_failures);
    return 1;
  }
  std::printf("vulkan_frame_graph_recorder_test OK\n");
  return 0;
}
