#pragma once

#include "runtime/function/render/rhi/rhi_types.h"
#include "runtime/function/render/slang/shader_resource_layout.h"

namespace Blunder {

class WindowSystem;

namespace rhi {

struct RenderDeviceDesc {
  WindowSystem* window_system{nullptr};
  bool enable_validation{true};
};

struct OffscreenTargetDesc {
  uint32_t width{0};
  uint32_t height{0};
  PixelFormat color_format{PixelFormat::R8G8B8A8_UNORM};
};

struct BufferDesc {
  uint64_t size{0};
  bool host_visible{false};
  bool uniform_buffer{false};
  bool copy_source{false};
};

struct GraphicsPipelineDesc {
  const char* shader_path{"engine/shaders/basic.slang"};
  bool enable_vertex_input{true};
  PrimitiveTopology topology{PrimitiveTopology::TriangleList};
  CullMode cull_mode{CullMode::Back};
  bool enable_blend{false};
  bool enable_depth_test{false};
  bool enable_depth_write{false};
  CompareOp depth_compare_op{CompareOp::LessOrEqual};
  bool enable_depth_bias{false};
  float depth_bias_constant_factor{0.0f};
  float depth_bias_slope_factor{0.0f};
  bool enable_skinned_vertex_input{false};
  uint32_t expected_descriptor_bindings[k_max_expected_descriptor_bindings]{0};
  uint32_t expected_descriptor_sets[k_max_expected_descriptor_bindings]{0};
  uint32_t expected_descriptor_binding_count{1};
  /// Non-zero: reuse an existing VkDescriptorSetLayout (opaque mesh layout).
  uint64_t shared_descriptor_set_layout{0};
  bool depth_only_subpass{false};
};

struct RenderBackendInitInfo {
  RenderDeviceDesc device_desc{};
};

}  // namespace rhi
}  // namespace Blunder
