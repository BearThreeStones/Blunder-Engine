#include "runtime/function/render/overlay/grid_overlay.h"

#include <cmath>
#include <vulkan/vulkan.h>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/overlay/overlay_resources.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/render/rhi/i_offscreen_render_target.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_shader.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"

#include <slang.h>

namespace Blunder {

namespace {

VkPipeline createGridLineMrtPipeline(
    VkDevice device, VkRenderPass render_pass,
    VkPipelineLayout pipeline_layout,
    const eastl::vector<VulkanShader::ShaderStage>& stages) {
  eastl::vector<VkPipelineShaderStageCreateInfo> stage_infos;
  for (const VulkanShader::ShaderStage& stage : stages) {
    VkPipelineShaderStageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage = stage.stage_flags;
    info.module = stage.module;
    info.pName = stage.entry_point.c_str();
    stage_infos.push_back(info);
  }

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;
  rasterizer.depthBiasEnable = VK_TRUE;
  rasterizer.depthBiasConstantFactor = -1.2f;
  rasterizer.depthBiasSlopeFactor = -1.2f;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depth_stencil{};
  depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_FALSE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

  VkPipelineColorBlendAttachmentState blend_attachments[2]{};
  for (uint32_t i = 0; i < 2; ++i) {
    blend_attachments[i].blendEnable = VK_TRUE;
    blend_attachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend_attachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachments[i].colorBlendOp = VK_BLEND_OP_ADD;
    blend_attachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend_attachments[i].alphaBlendOp = VK_BLEND_OP_ADD;
    blend_attachments[i].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  }

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.attachmentCount = 2;
  color_blending.pAttachments = blend_attachments;

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR,
                                     VK_DYNAMIC_STATE_DEPTH_BIAS};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 3;
  dynamic_state.pDynamicStates = dynamic_states;

  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = static_cast<uint32_t>(stage_infos.size());
  pipeline_info.pStages = stage_infos.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = render_pass;
  pipeline_info.subpass = 0;

  VkPipeline pipeline = VK_NULL_HANDLE;
  const VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[GridOverlay] line MRT vkCreateGraphicsPipelines failed: {}",
              static_cast<int>(result));
  }
  return pipeline;
}


// Must match the shader constants.
constexpr uint32_t k_grid_steps_len = 8;
constexpr uint32_t k_grid_steps_draw = 3;

struct GridUniformData {
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec4 camera_position_and_proj_type;
  glm::vec4 camera_forward_and_far_clip;
  glm::vec4 plane_origin;
  glm::vec4 plane_axis_u;
  glm::vec4 plane_axis_v;
  glm::vec4 steps[k_grid_steps_len];  // per-level step sizes: [i].xy = stepU, stepV
  // params: x=level(float), y=numLinesPerLevel, z=alphaScale, w=pixelFac
  glm::vec4 params;
  glm::vec4 axis_color_x;
  glm::vec4 axis_color_y;
  glm::vec4 axis_color_z;
};

// Default step sizes (powers of 10, scene units).
// steps[0] = 0.01, steps[1] = 0.1, steps[2] = 1.0, ...
constexpr float k_grid_default_steps[k_grid_steps_len] = {
    0.01f, 0.1f, 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f};

constexpr float k_grid_base_alpha = 0.28f;
constexpr uint32_t k_grid_lines_per_axis = 96;
constexpr uint32_t k_grid_axis_line_vertices = 6u;
constexpr float k_grid_axis_alpha = 0.95f;

/// Compute the float `level` by iterating through the steps array to find
/// where `dist` falls between two adjacent step sizes.  The integer part
/// is the level index and the fractional part drives smooth cross-fade.
float computeGridLevel(const float* steps, float dist) {
  for (int i = 0; i < static_cast<int>(k_grid_steps_len); ++i) {
    const float curr = steps[i];
    const float next = (i < static_cast<int>(k_grid_steps_len) - 1)
                           ? steps[i + 1]
                           : curr * 10.0f;
    if (next >= dist || i == static_cast<int>(k_grid_steps_len) - 1) {
      const float denom = next - curr;
      const float frac =
          (denom > 1e-8f) ? (dist - curr) / denom : 0.0f;
      return static_cast<float>(i) + std::clamp(frac, 0.0f, 1.0f);
    }
  }
  return 0.0f;
}

/// Compute a reference distance metric for the current camera.
/// Blender: interpolates between "directly below" and "looking at" distances
/// for perspective; uses pixel-derived scale for ortho.
float computeDistMetric(const OverlayState& state, const glm::vec3& plane_normal) {
  if (state.is_perspective) {
    // Distance from camera to the grid plane along the view direction.
    const float height = std::abs(glm::dot(state.camera_position, plane_normal));
    const float cos_angle = std::abs(glm::dot(state.camera_forward, plane_normal));
    // Interpolate between looking straight down (height) and along view dir
    // (height / cos), weighted by cos to avoid division by zero at grazing.
    const float view_dist =
        (cos_angle > 1e-4f) ? height / cos_angle : height * 1000.0f;
    // Blend: at steep angles use direct height, at shallow use view distance.
    return view_dist * (1.0f - cos_angle) + height * cos_angle;
  }
  // Orthographic: scale from ortho size.
  return state.ortho_size;
}

}  // namespace

GridOverlay::~GridOverlay() {
  shutdown();
}

void GridOverlay::initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                             rhi::IOffscreenRenderTarget* offscreen,
                             SlangCompiler* compiler) {
  ASSERT(ctx);
  ASSERT(alloc);
  ASSERT(offscreen);
  ASSERT(compiler);

  m_vk_context = ctx;
  m_vk_allocator = alloc;
  m_slang_compiler = compiler;

  // Create the grid pipeline (self-owned).
  rhi::GraphicsPipelineDesc grid_pipeline_desc{};
  grid_pipeline_desc.shader_path = "engine/shaders/grid.slang";
  grid_pipeline_desc.enable_vertex_input = false;
  grid_pipeline_desc.topology = rhi::PrimitiveTopology::LineList;
  grid_pipeline_desc.cull_mode = rhi::CullMode::None;
  grid_pipeline_desc.enable_blend = true;
  grid_pipeline_desc.enable_depth_test = true;
  grid_pipeline_desc.enable_depth_write = false;
  grid_pipeline_desc.enable_depth_bias = true;
  grid_pipeline_desc.depth_bias_constant_factor = -1.2f;
  grid_pipeline_desc.depth_bias_slope_factor = -1.2f;
  m_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_pipeline->bind(ctx, compiler);
  m_pipeline->initialize(*offscreen, grid_pipeline_desc);

  // Allocate per-frame uniform buffers.
  const uint32_t frames = VulkanSync::k_max_frames_in_flight;
  m_uniform_buffers.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_uniform_buffers[i]->create(m_vk_allocator, sizeof(GridUniformData),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  // Create descriptor pool and sets.
  VkDevice device = m_vk_context->getDevice();

  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = frames;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  pool_info.maxSets = frames;
  VkDescriptorPool pool = VK_NULL_HANDLE;
  const VkResult pool_result =
      vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
  if (pool_result != VK_SUCCESS) {
    LOG_FATAL("[GridOverlay] vkCreateDescriptorPool failed: {}",
              static_cast<int>(pool_result));
  }
  m_descriptor_pool = reinterpret_cast<uintptr_t>(pool);

  const VkDescriptorSetLayout layout =
      m_pipeline->nativePipeline()->getDescriptorSetLayout();
  eastl::vector<VkDescriptorSetLayout> layouts(frames, layout);
  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = frames;
  alloc_info.pSetLayouts = layouts.data();

  eastl::vector<VkDescriptorSet> sets(frames);
  const VkResult set_result =
      vkAllocateDescriptorSets(device, &alloc_info, sets.data());
  if (set_result != VK_SUCCESS) {
    LOG_FATAL("[GridOverlay] vkAllocateDescriptorSets failed: {}",
              static_cast<int>(set_result));
  }
  m_descriptor_sets.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    m_descriptor_sets[i] = reinterpret_cast<uintptr_t>(sets[i]);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = m_uniform_buffers[i]->getBuffer();
    buffer_info.offset = 0;
    buffer_info.range = sizeof(GridUniformData);

    VkWriteDescriptorSet ubo_write{};
    ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ubo_write.dstSet = sets[i];
    ubo_write.dstBinding = 0;
    ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_write.descriptorCount = 1;
    ubo_write.pBufferInfo = &buffer_info;
    vkUpdateDescriptorSets(device, 1, &ubo_write, 0, nullptr);
  }
}

void GridOverlay::initializeLinePipeline(VkRenderPass line_render_pass,
                                         SlangCompiler* compiler) {
  ASSERT(m_vk_context);
  ASSERT(line_render_pass != VK_NULL_HANDLE);
  ASSERT(compiler);
  ASSERT(m_pipeline);

  if (m_line_pipeline != VK_NULL_HANDLE) {
    return;
  }

  m_line_pipeline_layout = m_pipeline->nativePipeline()->getPipelineLayout();

  eastl::vector<VulkanShader::EntryPointSpec> entries;
  entries.push_back(
      {"vertexMain", VK_SHADER_STAGE_VERTEX_BIT, SLANG_STAGE_VERTEX});
  entries.push_back({"fragmentLineMrt", VK_SHADER_STAGE_FRAGMENT_BIT,
                     SLANG_STAGE_FRAGMENT});

  auto stages = VulkanShader::loadFromSlang(
      m_vk_context->getDevice(), compiler, "engine/shaders/grid_line.slang",
      entries);
  m_line_pipeline = createGridLineMrtPipeline(
      m_vk_context->getDevice(), line_render_pass, m_line_pipeline_layout,
      stages);
  for (auto& stage : stages) {
    VulkanShader::destroyShaderModule(m_vk_context->getDevice(), &stage.module);
  }
}

void GridOverlay::shutdown() {
  if (m_vk_context == nullptr) {
    return;
  }

  VkDevice device = m_vk_context->getDevice();

  if (m_line_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, m_line_pipeline, nullptr);
    m_line_pipeline = VK_NULL_HANDLE;
  }
  m_line_pipeline_layout = VK_NULL_HANDLE;

  for (auto& buf : m_uniform_buffers) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_uniform_buffers.clear();

  m_descriptor_sets.clear();
  if (m_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        device, reinterpret_cast<VkDescriptorPool>(m_descriptor_pool),
        nullptr);
    m_descriptor_pool = 0;
  }

  if (m_pipeline) {
    m_pipeline->shutdown();
    m_pipeline.reset();
  }

  m_slang_compiler = nullptr;
  m_vk_allocator = nullptr;
  m_vk_context = nullptr;
}

void GridOverlay::begin_sync(OverlayResources& /*res*/,
                             const OverlayState& /*state*/) {
  enabled_ = true;
}

void GridOverlay::draw_line(VkCommandBuffer cmd,
                            const OverlayState& state) {
  if (!enabled_ || m_pipeline == nullptr) {
    return;
  }

  const uint32_t frame_index = state.frame_index;

  GridUniformData grid_ubo{};
  grid_ubo.view = state.view;
  grid_ubo.projection = state.projection;

  const bool is_ortho = !state.is_perspective;
  grid_ubo.camera_position_and_proj_type =
      glm::vec4(state.camera_position, is_ortho ? 1.0f : 0.0f);
  grid_ubo.camera_forward_and_far_clip =
      glm::vec4(state.camera_forward, state.far_clip);

  // Set up grid plane.
  glm::vec3 plane_normal;
  switch (state.grid_plane) {
    case ForwardGridPlane::xy:
      grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
      grid_ubo.plane_origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      plane_normal = glm::vec3(0.0f, 0.0f, 1.0f);
      break;
    case ForwardGridPlane::yz:
      grid_ubo.plane_axis_u = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
      grid_ubo.plane_origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      plane_normal = glm::vec3(1.0f, 0.0f, 0.0f);
      break;
    case ForwardGridPlane::xz:
    default:
      grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
      grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
      grid_ubo.plane_origin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
      plane_normal = glm::vec3(0.0f, 1.0f, 0.0f);
      break;
  }

  // Fill steps array.
  for (uint32_t i = 0; i < k_grid_steps_len; ++i) {
    const float s = k_grid_default_steps[i];
    grid_ubo.steps[i] = glm::vec4(s, s, 0.0f, 0.0f);
  }

  // Compute reference distance and float level.
  const float dist = computeDistMetric(state, plane_normal);
  const float level = computeGridLevel(k_grid_default_steps, dist);

  // Store camera offset on plane_origin so the shader can snap per-level.
  const glm::vec3 axis_u = glm::vec3(grid_ubo.plane_axis_u);
  const glm::vec3 axis_v = glm::vec3(grid_ubo.plane_axis_v);
  const float cam_u = glm::dot(state.camera_position, axis_u);
  const float cam_v = glm::dot(state.camera_position, axis_v);
  grid_ubo.plane_origin =
      glm::vec4(cam_u * axis_u + cam_v * axis_v, 1.0f);

  // Pixel size factor for orthographic fade.
  const float pixel_fac =
      is_ortho
          ? (state.ortho_size * 2.0f /
             std::max(static_cast<float>(state.viewport_height), 1.0f))
          : 0.0f;

  // Vertices per level: linesPerAxis lines × 2 axes × 2 vertices/line.
  const uint32_t num_lines_per_level = k_grid_lines_per_axis * 4u;
  const uint32_t total_grid_vertices = k_grid_steps_draw * num_lines_per_level;

  grid_ubo.params = glm::vec4(
      level,
      static_cast<float>(num_lines_per_level),
      k_grid_base_alpha,
      pixel_fac);
  grid_ubo.axis_color_x = glm::vec4(kAxisColorPositiveX, k_grid_axis_alpha);
  grid_ubo.axis_color_y = glm::vec4(kAxisColorPositiveY, k_grid_axis_alpha);
  grid_ubo.axis_color_z = glm::vec4(kAxisColorPositiveZ, k_grid_axis_alpha);

  // Upload uniform data.
  m_uniform_buffers[frame_index]->upload(&grid_ubo, sizeof(grid_ubo));

  const VkPipeline pipeline =
      m_line_pipeline != VK_NULL_HANDLE
          ? m_line_pipeline
          : m_pipeline->nativePipeline()->getGraphicsPipeline();
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  const VkDescriptorSet descriptor_set = reinterpret_cast<VkDescriptorSet>(
      m_descriptor_sets[frame_index]);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipeline->nativePipeline()->getPipelineLayout(),
                          0, 1, &descriptor_set, 0, nullptr);

  // Single draw call: 3 levels of grid + 3 axis lines.
  vkCmdSetDepthBias(cmd, -1.2f, 0.0f, -1.2f);
  vkCmdDraw(cmd, total_grid_vertices + k_grid_axis_line_vertices, 1, 0, 0);
}

}  // namespace Blunder
