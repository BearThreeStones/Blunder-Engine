#include "runtime/function/render/render_system.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_scancode.h>
#include <slang.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "EASTL/memory.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/event/event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/function/render/debug/renderdoc_capture.h"
#include "runtime/function/render/editor_camera.h"
#include <vulkan/vulkan.h>

#include "runtime/function/render/offscreen_render_target.h"
#include "runtime/function/render/presenter/i_viewport_presenter.h"
#include "runtime/function/render/rhi/i_render_backend.h"
#include "runtime/function/render/rhi/render_backend_factory.h"
#include "runtime/function/render/rhi/rhi_desc.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_pipeline.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"
#include "runtime/function/render/vulkan_backend/vulkan_graphics_pipeline.h"
#include "runtime/function/render/vulkan_backend/vulkan_offscreen_target.h"
#include "runtime/function/render/vulkan_backend/vulkan_render_backend.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {

namespace {

const uint64_t k_fence_wait_timeout_ns = 1000000000ULL;

struct GridUniformData {
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec4 camera_position_and_proj_type;
  glm::vec4 camera_forward_and_far_clip;
  glm::vec4 plane_origin;
  glm::vec4 plane_axis_u;
  glm::vec4 plane_axis_v;
  glm::vec4 params0;  // step, major_scale, half_extent, line_count
  glm::vec4 params1;  // edge_fade, angle_fade, far_fade, stipple_scale
  glm::vec4 params2;  // stipple_duty, alpha_scale, iteration, reserved
};

struct MeshUniformData {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
};

struct DemoMeshData {
  eastl::vector<Vertex> vertices;
  eastl::vector<uint32_t> indices;
};

constexpr uint32_t k_default_viewport_w = 1024;
constexpr uint32_t k_default_viewport_h = 720;
constexpr float k_grid_major_scale = 10.0f;
constexpr float k_grid_half_extent = 120.0f;
constexpr float k_grid_base_alpha = 0.22f;
constexpr float k_grid_minor_alpha = 0.12f;
constexpr float k_grid_edge_fade = 1.45f;
constexpr float k_grid_angle_fade = 1.65f;
constexpr float k_grid_far_fade = 1.0f;
constexpr float k_grid_stipple_scale = 1.5f;
constexpr float k_grid_stipple_duty = 0.6f;
constexpr uint32_t k_grid_iterations = 3;
constexpr uint32_t k_grid_lines_per_axis = 96;
constexpr uint32_t k_smoke_texture_size = 64;
constexpr float k_demo_mesh_spin_rate = 0.85f;
constexpr float k_demo_mesh_height = 0.75f;
constexpr float k_demo_mesh_uniform_scale = 0.85f;
constexpr const char* k_demo_mesh_asset_path =
  "models/textured_cube/textured_cube.gltf";

DemoMeshData buildDemoCubeMesh() {
  constexpr float half_extent = 0.5f;

  DemoMeshData mesh;
  mesh.vertices = {
      {{-half_extent, -half_extent, -half_extent}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
      {{ half_extent, -half_extent, -half_extent}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
      {{ half_extent,  half_extent, -half_extent}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
      {{-half_extent,  half_extent, -half_extent}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},

      {{-half_extent,  half_extent,  half_extent}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
      {{ half_extent,  half_extent,  half_extent}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
      {{ half_extent, -half_extent,  half_extent}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
      {{-half_extent, -half_extent,  half_extent}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

      {{-half_extent,  half_extent, -half_extent}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
      {{ half_extent,  half_extent, -half_extent}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
      {{ half_extent,  half_extent,  half_extent}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
      {{-half_extent,  half_extent,  half_extent}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

      {{ half_extent, -half_extent, -half_extent}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
      {{-half_extent, -half_extent, -half_extent}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
      {{-half_extent, -half_extent,  half_extent}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
      {{ half_extent, -half_extent,  half_extent}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},

      {{-half_extent, -half_extent, -half_extent}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
      {{-half_extent,  half_extent, -half_extent}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
      {{-half_extent,  half_extent,  half_extent}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
      {{-half_extent, -half_extent,  half_extent}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},

      {{ half_extent,  half_extent, -half_extent}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
      {{ half_extent, -half_extent, -half_extent}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
      {{ half_extent, -half_extent,  half_extent}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
      {{ half_extent,  half_extent,  half_extent}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
  };

  mesh.indices = {
      0u, 1u, 2u, 0u, 2u, 3u,
      4u, 5u, 6u, 4u, 6u, 7u,
      8u, 9u, 10u, 8u, 10u, 11u,
      12u, 13u, 14u, 12u, 14u, 15u,
      16u, 17u, 18u, 16u, 18u, 19u,
      20u, 21u, 22u, 20u, 22u, 23u,
  };

  return mesh;
}

eastl::vector<uint8_t> buildSmokeCheckerboardPixels(uint32_t width,
                                                    uint32_t height) {
  eastl::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4u);

  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      const bool dark_tile = (((x / 8u) + (y / 8u)) & 1u) == 0u;
      const size_t index =
          (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u;
      pixels[index + 0] = dark_tile ? 36u : 220u;
      pixels[index + 1] = dark_tile ? 92u : 184u;
      pixels[index + 2] = dark_tile ? 208u : 52u;
      pixels[index + 3] = 255u;
    }
  }

  return pixels;
}

vulkan_backend::VulkanRenderBackend* vkBackend(RenderSystem* self) {
  return static_cast<vulkan_backend::VulkanRenderBackend*>(
      self->getRenderBackend());
}

VulkanContext* vkCtx(RenderSystem* self) {
  return vkBackend(self)->nativeVulkanContext();
}

VulkanAllocator* vkAlloc(RenderSystem* self) {
  return vkBackend(self)->nativeAllocator();
}

VulkanSync* vkSync(RenderSystem* self) {
  return vkBackend(self)->nativeSync();
}

OffscreenRenderTarget* vkOffscreenRt(RenderSystem* self) {
  auto* target = static_cast<vulkan_backend::VulkanOffscreenTarget*>(
      self->getOffscreenTarget());
  return target ? target->nativeTarget() : nullptr;
}

}  // namespace

RenderSystem::RenderSystem() = default;

RenderSystem::~RenderSystem() { shutdown(); }

bool RenderSystem::isVulkanBackend() const {
  return m_backend && m_backend->type() == rhi::RenderBackendType::Vulkan;
}

void RenderSystem::initialize(const RenderSystemInitInfo& info) {
  ASSERT(info.window_system);

  m_asset_manager = info.asset_manager;
  m_window_system = info.window_system;
  m_viewport_presenter = info.viewport_presenter;
  m_viewport_layout_source = info.viewport_layout_source;

  rhi::RenderBackendInitInfo backend_init{};
  backend_init.device_desc.window_system = info.window_system;
  backend_init.device_desc.enable_validation = info.enable_validation;
  m_backend = rhi::RenderBackendFactory::createFromSettings(backend_init);

  if (m_backend->type() == rhi::RenderBackendType::D3D12) {
    initializeD3D12SkeletonPath(info);
    return;
  }
  initializeVulkanPath(info);
}

void RenderSystem::initializeD3D12SkeletonPath(
    const RenderSystemInitInfo& info) {
  (void)info;
  LOG_WARN(
      "[RenderSystem] D3D12 skeleton backend: scene pipelines are not "
      "implemented in P0");

  rhi::OffscreenTargetDesc offscreen_desc{};
  offscreen_desc.width = k_default_viewport_w;
  offscreen_desc.height = k_default_viewport_h;
  m_offscreen = m_backend->device().createOffscreenTarget(offscreen_desc);
  m_editor_camera = eastl::make_unique<EditorCamera>(m_window_system);
}

void RenderSystem::initializeVulkanPath(const RenderSystemInitInfo& info) {
  (void)info;

  rhi::OffscreenTargetDesc offscreen_desc{};
  offscreen_desc.width = k_default_viewport_w;
  offscreen_desc.height = k_default_viewport_h;
  m_offscreen = vkBackend(this)->device().createOffscreenTarget(offscreen_desc);

  rhi::GraphicsPipelineDesc mesh_pipeline_desc{};
  mesh_pipeline_desc.shader_path = "engine/shaders/basic.slang";
  mesh_pipeline_desc.enable_vertex_input = true;
  mesh_pipeline_desc.cull_mode = rhi::CullMode::None;
  mesh_pipeline_desc.enable_depth_test = true;
  mesh_pipeline_desc.enable_depth_write = true;
  mesh_pipeline_desc.enable_texture_sampling = true;
  m_mesh_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_mesh_pipeline->bind(vkCtx(this), vkBackend(this)->nativeSlangCompiler());
  m_mesh_pipeline->initialize(*m_offscreen, mesh_pipeline_desc);

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
  m_grid_pipeline = eastl::make_unique<vulkan_backend::VulkanGraphicsPipeline>();
  m_grid_pipeline->bind(vkCtx(this), vkBackend(this)->nativeSlangCompiler());
  m_grid_pipeline->initialize(*m_offscreen, grid_pipeline_desc);

  m_editor_camera = eastl::make_unique<EditorCamera>(m_window_system);

  m_mesh_uniform_buffers.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_mesh_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_mesh_uniform_buffers[i]->create(
        vkAlloc(this), sizeof(MeshUniformData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  m_grid_uniform_buffers.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_grid_uniform_buffers[i] = eastl::make_unique<VulkanBuffer>();
    m_grid_uniform_buffers[i]->create(
        vkAlloc(this), sizeof(GridUniformData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  VkDescriptorPoolSize mesh_pool_sizes[3]{};
  mesh_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  mesh_pool_sizes[0].descriptorCount = VulkanSync::k_max_frames_in_flight;
  mesh_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  mesh_pool_sizes[1].descriptorCount = VulkanSync::k_max_frames_in_flight;
  mesh_pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  mesh_pool_sizes[2].descriptorCount = VulkanSync::k_max_frames_in_flight;

  VkDescriptorPoolCreateInfo mesh_pool_info{};
  mesh_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  mesh_pool_info.poolSizeCount = 3;
  mesh_pool_info.pPoolSizes = mesh_pool_sizes;
  mesh_pool_info.maxSets = VulkanSync::k_max_frames_in_flight;
  VkDescriptorPool mesh_pool = VK_NULL_HANDLE;
  const VkResult mesh_pool_result =
      vkCreateDescriptorPool(vkCtx(this)->getDevice(), &mesh_pool_info, nullptr,
                             &mesh_pool);
  if (mesh_pool_result != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::initialize] mesh vkCreateDescriptorPool failed: {}",
        static_cast<int>(mesh_pool_result));
  }
  m_mesh_descriptor_pool = reinterpret_cast<uintptr_t>(mesh_pool);

  eastl::vector<VkDescriptorSetLayout> mesh_layouts(
      VulkanSync::k_max_frames_in_flight,
      m_mesh_pipeline->nativePipeline()->getDescriptorSetLayout());
  VkDescriptorSetAllocateInfo mesh_alloc_info{};
  mesh_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  mesh_alloc_info.descriptorPool = mesh_pool;
  mesh_alloc_info.descriptorSetCount = VulkanSync::k_max_frames_in_flight;
  mesh_alloc_info.pSetLayouts = mesh_layouts.data();

  eastl::vector<VkDescriptorSet> mesh_sets(VulkanSync::k_max_frames_in_flight);
  const VkResult mesh_set_result = vkAllocateDescriptorSets(
      vkCtx(this)->getDevice(), &mesh_alloc_info, mesh_sets.data());
  if (mesh_set_result != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::initialize] mesh vkAllocateDescriptorSets failed: {}",
        static_cast<int>(mesh_set_result));
  }
  m_mesh_descriptor_sets.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_mesh_descriptor_sets[i] = reinterpret_cast<uintptr_t>(mesh_sets[i]);
  }

  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    VkDescriptorBufferInfo mesh_buffer_info{};
    mesh_buffer_info.buffer = m_mesh_uniform_buffers[i]->getBuffer();
    mesh_buffer_info.offset = 0;
    mesh_buffer_info.range = sizeof(MeshUniformData);

    VkWriteDescriptorSet mesh_descriptor_write{};
    mesh_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    mesh_descriptor_write.dstSet =
        reinterpret_cast<VkDescriptorSet>(m_mesh_descriptor_sets[i]);
    mesh_descriptor_write.dstBinding = 0;
    mesh_descriptor_write.dstArrayElement = 0;
    mesh_descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    mesh_descriptor_write.descriptorCount = 1;
    mesh_descriptor_write.pBufferInfo = &mesh_buffer_info;
    vkUpdateDescriptorSets(vkCtx(this)->getDevice(), 1, &mesh_descriptor_write,
                           0, nullptr);
  }

  VkDescriptorPoolSize grid_pool_size{};
  grid_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  grid_pool_size.descriptorCount = VulkanSync::k_max_frames_in_flight;

  VkDescriptorPoolCreateInfo grid_pool_info{};
  grid_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  grid_pool_info.poolSizeCount = 1;
  grid_pool_info.pPoolSizes = &grid_pool_size;
  grid_pool_info.maxSets = VulkanSync::k_max_frames_in_flight;
  VkDescriptorPool grid_pool = VK_NULL_HANDLE;
  const VkResult grid_pool_result =
      vkCreateDescriptorPool(vkCtx(this)->getDevice(), &grid_pool_info, nullptr,
                             &grid_pool);
  if (grid_pool_result != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::initialize] grid vkCreateDescriptorPool failed: {}",
        static_cast<int>(grid_pool_result));
  }
  m_grid_descriptor_pool = reinterpret_cast<uintptr_t>(grid_pool);

  eastl::vector<VkDescriptorSetLayout> grid_layouts(
      VulkanSync::k_max_frames_in_flight,
      m_grid_pipeline->nativePipeline()->getDescriptorSetLayout());
  VkDescriptorSetAllocateInfo grid_alloc_info{};
  grid_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  grid_alloc_info.descriptorPool = grid_pool;
  grid_alloc_info.descriptorSetCount = VulkanSync::k_max_frames_in_flight;
  grid_alloc_info.pSetLayouts = grid_layouts.data();

  eastl::vector<VkDescriptorSet> grid_sets(VulkanSync::k_max_frames_in_flight);
  const VkResult grid_set_result = vkAllocateDescriptorSets(
      vkCtx(this)->getDevice(), &grid_alloc_info, grid_sets.data());
  if (grid_set_result != VK_SUCCESS) {
    LOG_FATAL(
        "[RenderSystem::initialize] grid vkAllocateDescriptorSets failed: {}",
        static_cast<int>(grid_set_result));
  }
  m_grid_descriptor_sets.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_grid_descriptor_sets[i] = reinterpret_cast<uintptr_t>(grid_sets[i]);
  }

  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    VkDescriptorBufferInfo grid_buffer_info{};
    grid_buffer_info.buffer = m_grid_uniform_buffers[i]->getBuffer();
    grid_buffer_info.offset = 0;
    grid_buffer_info.range = sizeof(GridUniformData);

    VkWriteDescriptorSet grid_descriptor_write{};
    grid_descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    grid_descriptor_write.dstSet =
        reinterpret_cast<VkDescriptorSet>(m_grid_descriptor_sets[i]);
    grid_descriptor_write.dstBinding = 0;
    grid_descriptor_write.dstArrayElement = 0;
    grid_descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    grid_descriptor_write.descriptorCount = 1;
    grid_descriptor_write.pBufferInfo = &grid_buffer_info;
    vkUpdateDescriptorSets(vkCtx(this)->getDevice(), 1, &grid_descriptor_write, 0,
                           nullptr);
  }

  Asset::Meta smoke_meta;
  smoke_meta.virtual_path = "generated://render/smoke_checkerboard";
  Texture2DAsset smoke_asset(smoke_meta, k_smoke_texture_size,
                             k_smoke_texture_size, 4u,
                             buildSmokeCheckerboardPixels(k_smoke_texture_size,
                                                          k_smoke_texture_size));
  VulkanTexture* mesh_texture = ensureTextureUploaded(&smoke_asset);

  const eastl::shared_ptr<MeshAsset> imported_mesh =
      m_asset_manager != nullptr
          ? m_asset_manager->loadMesh(k_demo_mesh_asset_path)
          : nullptr;

  const void* mesh_vertex_bytes = nullptr;
  VkDeviceSize mesh_vertex_byte_size = 0;
  const uint32_t* mesh_indices = nullptr;
  size_t mesh_index_count = 0;

  DemoMeshData fallback_demo_mesh;
  if (imported_mesh && imported_mesh->getVertexCount() > 0 &&
      imported_mesh->getIndexCount() > 0) {
    mesh_vertex_bytes = imported_mesh->getVertexData();
    mesh_vertex_byte_size =
        static_cast<VkDeviceSize>(imported_mesh->getVertexByteSize());
    mesh_indices = imported_mesh->getIndices().data();
    mesh_index_count = imported_mesh->getIndexCount();
    LOG_INFO(
        "[RenderSystem] loaded demo mesh asset {} (vertices={}, indices={}, material={})",
        k_demo_mesh_asset_path, imported_mesh->getVertexCount(),
        imported_mesh->getIndexCount(),
        imported_mesh->hasMaterial() ? "yes" : "no");

    if (imported_mesh->getMaterialAsset() != nullptr &&
        imported_mesh->getMaterialAsset()->getBaseColorTextureAsset() != nullptr) {
      VulkanTexture* material_texture = ensureTextureUploaded(
          imported_mesh->getMaterialAsset()->getBaseColorTextureAsset().get());
      if (material_texture != nullptr) {
        mesh_texture = material_texture;
      }
    }
  } else {
    fallback_demo_mesh = buildDemoCubeMesh();
    mesh_vertex_bytes = fallback_demo_mesh.vertices.data();
    mesh_vertex_byte_size = static_cast<VkDeviceSize>(
        fallback_demo_mesh.vertices.size() * sizeof(Vertex));
    mesh_indices = fallback_demo_mesh.indices.data();
    mesh_index_count = fallback_demo_mesh.indices.size();
    LOG_INFO("[RenderSystem] using built-in fallback demo mesh");
  }

  m_demo_mesh_vertex_buffer = eastl::make_unique<VulkanBuffer>();
  m_demo_mesh_vertex_buffer->create(
      vkAlloc(this), mesh_vertex_byte_size,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_demo_mesh_vertex_buffer->upload(mesh_vertex_bytes, mesh_vertex_byte_size);

  m_demo_mesh_index_buffer = eastl::make_unique<VulkanBuffer>();
  m_demo_mesh_index_buffer->create(
      vkAlloc(this),
      static_cast<VkDeviceSize>(mesh_index_count * sizeof(uint32_t)),
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_demo_mesh_index_buffer->upload(
      mesh_indices,
      static_cast<VkDeviceSize>(mesh_index_count * sizeof(uint32_t)));
  m_demo_mesh_index_count = static_cast<uint32_t>(mesh_index_count);

  if (mesh_texture != nullptr) {
    for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
      VkDescriptorImageInfo sampled_image_info{};
      sampled_image_info.imageView = mesh_texture->getImageView();
      sampled_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      VkDescriptorImageInfo sampler_info{};
      sampler_info.sampler = mesh_texture->getSampler();

      VkWriteDescriptorSet texture_writes[2]{};
      texture_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      texture_writes[0].dstSet =
          reinterpret_cast<VkDescriptorSet>(m_mesh_descriptor_sets[i]);
      texture_writes[0].dstBinding = 1;
      texture_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      texture_writes[0].descriptorCount = 1;
      texture_writes[0].pImageInfo = &sampled_image_info;

      texture_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      texture_writes[1].dstSet =
          reinterpret_cast<VkDescriptorSet>(m_mesh_descriptor_sets[i]);
      texture_writes[1].dstBinding = 2;
      texture_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      texture_writes[1].descriptorCount = 1;
      texture_writes[1].pImageInfo = &sampler_info;

      vkUpdateDescriptorSets(vkCtx(this)->getDevice(), 2, texture_writes, 0,
                             nullptr);
    }
  }

  recreateReadbackStaging(k_default_viewport_w, k_default_viewport_h);

  // Best-effort RenderDoc hookup. If the engine wasn't launched from
  // RenderDoc, this is a silent no-op; otherwise F11 will capture one frame.
  m_renderdoc_capture = eastl::make_unique<RenderDocCapture>();
  m_renderdoc_capture->initialize();
}

void RenderSystem::recreateReadbackStaging(uint32_t width, uint32_t height) {
  if (!isVulkanBackend()) {
    return;
  }
  ASSERT(vkAlloc(this));
  if (width == 0 || height == 0) {
    return;
  }

  for (eastl::unique_ptr<VulkanBuffer>& buf : m_readback_staging) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_readback_staging.clear();

  const VkDeviceSize bytes = static_cast<VkDeviceSize>(width) * height * 4u;
  m_readback_staging.resize(VulkanSync::k_max_frames_in_flight);
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_readback_staging[i] = eastl::make_unique<VulkanBuffer>();
    m_readback_staging[i]->create(vkAlloc(this), bytes,
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VMA_MEMORY_USAGE_GPU_TO_CPU);
  }
  m_readback_width = width;
  m_readback_height = height;
  m_readback_pixels.resize(static_cast<size_t>(bytes));
}

VulkanTexture* RenderSystem::ensureTextureUploaded(
    const Texture2DAsset* texture_asset) {
  if (texture_asset == nullptr || !isVulkanBackend() || !vkAlloc(this)) {
    return nullptr;
  }

  eastl::string cache_key = texture_asset->getVirtualPath();
  if (cache_key.empty()) {
    const std::filesystem::path& absolute_path = texture_asset->getAbsolutePath();
    if (!absolute_path.empty()) {
      cache_key = eastl::string(absolute_path.generic_string().c_str());
    } else {
      cache_key = "generated://render/anonymous_texture";
    }
  }

  if (auto it = m_uploaded_textures.find(cache_key);
      it != m_uploaded_textures.end()) {
    return it->second.get();
  }

  auto uploaded_texture = eastl::make_unique<VulkanTexture>();
  uploaded_texture->createFromTexture2DAsset(vkCtx(this), vkAlloc(this),
                                             *texture_asset);
  VulkanTexture* uploaded_texture_ptr = uploaded_texture.get();
  m_uploaded_textures[cache_key] = eastl::move(uploaded_texture);

  LOG_INFO("[RenderSystem] texture uploaded {} ({}x{}, {} bytes)",
           cache_key.c_str(), texture_asset->getWidth(),
           texture_asset->getHeight(), texture_asset->getPixelByteSize());
  return uploaded_texture_ptr;
}

void RenderSystem::resizeOffscreenIfNeeded(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0 || !m_offscreen) {
    return;
  }
  const rhi::Extent2D current = m_offscreen->extent();
  if (current.width == width && current.height == height) {
    return;
  }

  if (isVulkanBackend()) {
    vkDeviceWaitIdle(vkCtx(this)->getDevice());
    recreateReadbackStaging(width, height);
  }
  m_offscreen->resize(width, height);
}

void RenderSystem::shutdown() {
  if (!m_backend) {
    return;
  }

  if (isVulkanBackend()) {
    vkDeviceWaitIdle(vkCtx(this)->getDevice());
  }

  if (m_renderdoc_capture) {
    m_renderdoc_capture->shutdown();
    m_renderdoc_capture.reset();
  }

  for (eastl::unique_ptr<VulkanBuffer>& buf : m_readback_staging) {
    if (buf) {
      buf->destroy();
      buf.reset();
    }
  }
  m_readback_staging.clear();
  m_readback_pixels.clear();

  for (auto& [key, texture] : m_uploaded_textures) {
    if (texture) {
      texture->destroy();
      texture.reset();
    }
  }
  m_uploaded_textures.clear();

  if (m_demo_mesh_index_buffer) {
    m_demo_mesh_index_buffer->destroy();
    m_demo_mesh_index_buffer.reset();
  }

  if (m_demo_mesh_vertex_buffer) {
    m_demo_mesh_vertex_buffer->destroy();
    m_demo_mesh_vertex_buffer.reset();
  }

  if (m_offscreen) {
    if (auto* vk_target =
            static_cast<vulkan_backend::VulkanOffscreenTarget*>(m_offscreen.get())) {
      vk_target->shutdown();
    }
    m_offscreen.reset();
  }

  for (eastl::unique_ptr<VulkanBuffer>& uniform_buffer :
       m_mesh_uniform_buffers) {
    if (uniform_buffer) {
      uniform_buffer->destroy();
      uniform_buffer.reset();
    }
  }
  m_mesh_uniform_buffers.clear();

  for (eastl::unique_ptr<VulkanBuffer>& uniform_buffer :
       m_grid_uniform_buffers) {
    if (uniform_buffer) {
      uniform_buffer->destroy();
      uniform_buffer.reset();
    }
  }
  m_grid_uniform_buffers.clear();

  m_mesh_descriptor_sets.clear();

  if (m_mesh_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        vkCtx(this)->getDevice(),
        reinterpret_cast<VkDescriptorPool>(m_mesh_descriptor_pool), nullptr);
    m_mesh_descriptor_pool = 0;
  }

  m_grid_descriptor_sets.clear();

  if (m_grid_descriptor_pool != 0) {
    vkDestroyDescriptorPool(
        vkCtx(this)->getDevice(),
        reinterpret_cast<VkDescriptorPool>(m_grid_descriptor_pool), nullptr);
    m_grid_descriptor_pool = 0;
  }

  if (m_grid_pipeline) {
    m_grid_pipeline->shutdown();
    m_grid_pipeline.reset();
  }

  if (m_mesh_pipeline) {
    m_mesh_pipeline->shutdown();
    m_mesh_pipeline.reset();
  }

  m_editor_camera.reset();

  m_backend.reset();
  m_viewport_presenter = nullptr;

  m_window_system = nullptr;
  m_asset_manager = nullptr;
  m_demo_mesh_index_count = 0;
  m_demo_mesh_rotation_radians = 0.0f;
  m_current_frame = 0;
}

void RenderSystem::tick(float delta_time, uint32_t target_width,
                        uint32_t target_height) {
  if (!m_backend || !m_offscreen) {
    return;
  }
  if (m_backend->type() == rhi::RenderBackendType::D3D12) {
    tickD3D12Skeleton(delta_time, target_width, target_height);
    return;
  }
  tickVulkan(delta_time, target_width, target_height);
}

void RenderSystem::tickD3D12Skeleton(float delta_time, uint32_t target_width,
                                     uint32_t target_height) {
  (void)delta_time;
  resizeOffscreenIfNeeded(target_width, target_height);
  const rhi::Extent2D extent = m_offscreen->extent();
  if (extent.width == 0 || extent.height == 0) {
    return;
  }
  if (m_viewport_presenter) {
    m_readback_pixels.assign(static_cast<size_t>(extent.width) * extent.height * 4u,
                             0u);
    m_viewport_presenter->presentCpuRgba8(m_readback_pixels.data(), extent.width,
                                          extent.height, extent.width * 4u);
  }
}

void RenderSystem::tickVulkan(float delta_time, uint32_t target_width,
                              uint32_t target_height) {
  resizeOffscreenIfNeeded(target_width, target_height);

  const rhi::Extent2D offscreen_extent_rhi = m_offscreen->extent();
  const VkExtent2D offscreen_extent{offscreen_extent_rhi.width,
                                    offscreen_extent_rhi.height};
  if (offscreen_extent.width == 0 || offscreen_extent.height == 0) {
    return;
  }

  VkInstance instance = vkCtx(this)->getInstance();
  if (m_renderdoc_capture) {
    m_renderdoc_capture->beginFrame(instance);
  }

  glm::mat4 view(1.0f);
  glm::mat4 projection(1.0f);
  EditorCamera::ProjectionMode projection_mode =
      EditorCamera::ProjectionMode::perspective;
  glm::vec3 camera_position(2.0f, 2.0f, 2.0f);
  glm::vec3 camera_forward = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
  float camera_distance = 6.0f;
  float near_clip = 0.1f;
  float far_clip = 1000.0f;
  float vertical_fov = glm::radians(45.0f);
  float ortho_size = 10.0f;
  if (m_editor_camera) {
    int32_t viewport_x = 0;
    int32_t viewport_y = 0;
    if (m_viewport_layout_source) {
      const ViewportLogicalRect viewport_rect =
          m_viewport_layout_source->getViewportLogicalRect();
      viewport_x = viewport_rect.x;
      viewport_y = viewport_rect.y;
    }
    m_editor_camera->setViewportRect(
        viewport_x, viewport_y, static_cast<float>(offscreen_extent.width),
        static_cast<float>(offscreen_extent.height));
    m_editor_camera->onUpdate(delta_time);
    view = m_editor_camera->getViewMatrix();
    projection = m_editor_camera->getProjectionMatrix();
    projection_mode = m_editor_camera->getProjectionMode();
    camera_position = m_editor_camera->getPosition();
    camera_forward = m_editor_camera->getForwardDirection();
    camera_distance = m_editor_camera->getDistance();
    near_clip = m_editor_camera->getNearClip();
    far_clip = m_editor_camera->getFarClip();
    vertical_fov = m_editor_camera->getVerticalFov();
    ortho_size = m_editor_camera->getOrthoSize();
  } else {
    view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                       glm::vec3(0.0f, 0.0f, 1.0f));
    const float aspect = static_cast<float>(offscreen_extent.width) /
                         static_cast<float>(offscreen_extent.height);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
    proj[1][1] *= -1.0f;
    projection = proj;
    far_clip = 10.0f;
  }

  const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
  if (keyboard_state) {
    if (keyboard_state[SDL_SCANCODE_1]) {
      m_grid_plane = GridPlane::xy;
    } else if (keyboard_state[SDL_SCANCODE_2]) {
      m_grid_plane = GridPlane::xz;
    } else if (keyboard_state[SDL_SCANCODE_3]) {
      m_grid_plane = GridPlane::yz;
    }
  }

  VkDevice device = vkCtx(this)->getDevice();
  VkFence in_flight_fence = vkSync(this)->getInFlightFence(m_current_frame);
  vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE,
                  k_fence_wait_timeout_ns);
  vkResetFences(device, 1, &in_flight_fence);

  VkCommandBuffer command_buffer =
      m_grid_pipeline->nativePipeline()->getCommandBuffer(m_current_frame);
  vkResetCommandBuffer(command_buffer, 0);

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(command_buffer, &begin_info);

  m_demo_mesh_rotation_radians += delta_time * k_demo_mesh_spin_rate;
  if (m_demo_mesh_rotation_radians > 6.2831853f) {
    m_demo_mesh_rotation_radians -= 6.2831853f;
  }

  // Pass 1: 编辑器场景 -> 离屏渲染目标
  {
    VkClearValue clear_values[2]{};
    clear_values[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp_info{};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_info.renderPass = vkOffscreenRt(this)->getRenderPass();
    rp_info.framebuffer = vkOffscreenRt(this)->getFramebuffer();
    rp_info.renderArea.offset = {0, 0};
    rp_info.renderArea.extent = offscreen_extent;
    rp_info.clearValueCount = 2;
    rp_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(offscreen_extent.width);
    viewport.height = static_cast<float>(offscreen_extent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = offscreen_extent;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    GridUniformData grid_ubo{};
    grid_ubo.view = view;
    grid_ubo.projection = projection;
    grid_ubo.camera_position_and_proj_type = glm::vec4(
        camera_position,
        projection_mode == EditorCamera::ProjectionMode::orthographic ? 1.0f
                                                                      : 0.0f);
    grid_ubo.camera_forward_and_far_clip = glm::vec4(camera_forward, far_clip);
    switch (m_grid_plane) {
      case GridPlane::xy:
        grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        grid_ubo.plane_axis_v = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        grid_ubo.plane_origin = glm::vec4(0.0f, 0.0f, camera_position.z, 1.0f);
        break;
      case GridPlane::yz:
        grid_ubo.plane_axis_u = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        grid_ubo.plane_origin = glm::vec4(camera_position.x, 0.0f, 0.0f, 1.0f);
        break;
      case GridPlane::xz:
      default:
        grid_ubo.plane_axis_u = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        grid_ubo.plane_axis_v = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        grid_ubo.plane_origin = glm::vec4(0.0f, camera_position.y, 0.0f, 1.0f);
        break;
    }

    const float base_metric =
        projection_mode == EditorCamera::ProjectionMode::orthographic
            ? ortho_size
            : 2.0f * std::tan(vertical_fov * 0.5f) *
                  std::max(camera_distance, near_clip);
    const float step_power =
        std::floor(std::log10(std::max(base_metric * 0.1f, 0.0001f)));
    const float base_step = std::pow(10.0f, step_power);
    const float snapped_u =
        std::floor(glm::dot(glm::vec3(grid_ubo.plane_origin),
                            glm::vec3(grid_ubo.plane_axis_u)) /
                   base_step) *
        base_step;
    const float snapped_v =
        std::floor(glm::dot(glm::vec3(grid_ubo.plane_origin),
                            glm::vec3(grid_ubo.plane_axis_v)) /
                   base_step) *
        base_step;
    grid_ubo.plane_origin =
        snapped_u * grid_ubo.plane_axis_u + snapped_v * grid_ubo.plane_axis_v;
    grid_ubo.plane_origin.w = 1.0f;

    grid_ubo.params1 = glm::vec4(k_grid_edge_fade, k_grid_angle_fade,
                                 k_grid_far_fade, k_grid_stipple_scale);
    const uint32_t vertex_count = k_grid_lines_per_axis * 4u;
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_grid_pipeline->nativePipeline()->getGraphicsPipeline());
    const VkDescriptorSet grid_descriptor_set =
        reinterpret_cast<VkDescriptorSet>(
            m_grid_descriptor_sets[m_current_frame]);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           m_grid_pipeline->nativePipeline()->getPipelineLayout(), 0, 1,
                           &grid_descriptor_set, 0, nullptr);
    for (uint32_t iteration = 0; iteration < k_grid_iterations; ++iteration) {
      const float scale =
          std::pow(k_grid_major_scale, static_cast<float>(iteration));
      const float step = base_step * scale;
      const float alpha_scale =
          iteration == 0 ? k_grid_base_alpha : k_grid_minor_alpha / scale;
      grid_ubo.params0 =
          glm::vec4(step, k_grid_major_scale, k_grid_half_extent * scale,
                    static_cast<float>(k_grid_lines_per_axis));
      grid_ubo.params2 = glm::vec4(k_grid_stipple_duty, alpha_scale,
                                   static_cast<float>(iteration), 0.0f);
      m_grid_uniform_buffers[m_current_frame]->upload(&grid_ubo,
                                                      sizeof(grid_ubo));
      const float bias_scale = 1.0f + static_cast<float>(iteration) * 0.75f;
      vkCmdSetDepthBias(command_buffer, -1.2f * bias_scale, 0.0f,
                        -1.2f * bias_scale);
      vkCmdDraw(command_buffer, vertex_count, 1, 0, 0);
    }

    if (m_mesh_pipeline && m_demo_mesh_vertex_buffer && m_demo_mesh_index_buffer &&
        m_demo_mesh_index_count > 0 &&
        !m_mesh_uniform_buffers.empty() &&
        m_current_frame < m_mesh_descriptor_sets.size()) {
      MeshUniformData mesh_ubo{};
      glm::mat4 model = glm::translate(
          glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, k_demo_mesh_height));
      model = glm::rotate(model, m_demo_mesh_rotation_radians,
                          glm::vec3(0.0f, 0.0f, 1.0f));
      model = glm::scale(model,
                         glm::vec3(k_demo_mesh_uniform_scale,
                                   k_demo_mesh_uniform_scale,
                                   k_demo_mesh_uniform_scale));
      mesh_ubo.model = model;
      mesh_ubo.view = view;
      mesh_ubo.projection = projection;
      m_mesh_uniform_buffers[m_current_frame]->upload(&mesh_ubo,
                                                      sizeof(mesh_ubo));

      VkBuffer vertex_buffers[] = {m_demo_mesh_vertex_buffer->getBuffer()};
      VkDeviceSize vertex_offsets[] = {0};
      vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        m_mesh_pipeline->nativePipeline()->getGraphicsPipeline());
      const VkDescriptorSet mesh_descriptor_set =
          reinterpret_cast<VkDescriptorSet>(
              m_mesh_descriptor_sets[m_current_frame]);
      vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             m_mesh_pipeline->nativePipeline()->getPipelineLayout(), 0, 1,
                             &mesh_descriptor_set, 0, nullptr);
      vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers,
                             vertex_offsets);
      vkCmdBindIndexBuffer(command_buffer,
                           m_demo_mesh_index_buffer->getBuffer(), 0,
                           VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(command_buffer, m_demo_mesh_index_count, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);
    m_offscreen->markPostRenderPassShaderRead();
  }

  // 回读：SHADER_READ_ONLY -> TRANSFER_SRC -> copy -> SHADER_READ
  vkOffscreenRt(this)->cmdBarrierToTransferSrc(command_buffer);

  VkBufferImageCopy copy_region{};
  copy_region.bufferOffset = 0;
  copy_region.bufferRowLength = 0;
  copy_region.bufferImageHeight = 0;
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.mipLevel = 0;
  copy_region.imageSubresource.baseArrayLayer = 0;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageOffset = {0, 0, 0};
  copy_region.imageExtent.width = offscreen_extent.width;
  copy_region.imageExtent.height = offscreen_extent.height;
  copy_region.imageExtent.depth = 1;

  vkCmdCopyImageToBuffer(command_buffer, vkOffscreenRt(this)->getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         m_readback_staging[m_current_frame]->getBuffer(), 1,
                         &copy_region);

  vkOffscreenRt(this)->cmdBarrierToShaderRead(command_buffer);

  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  vkQueueSubmit(vkCtx(this)->getGraphicsQueue(), 1, &submit_info,
                in_flight_fence);

  // 同步等待，以便我们可以立即映射（Map）暂存缓冲区（Staging Buffer）
  // 这种方式虽然简单，但会导致流水线停顿（Stall）；
  // 如果性能分析（Profiling）显示此处存在瓶颈，清理阶段可以在
  // k_max_frames_in_flight 个缓冲区之间采用乒乓缓冲（Ping-ponging）机制
  vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE,
                  k_fence_wait_timeout_ns);
  // 映射缓冲区并将像素转发给 Slint
  if (m_viewport_presenter) {
    void* mapped = nullptr;
    const VkResult mr = vmaMapMemory(
        vkAlloc(this)->getAllocator(),
        m_readback_staging[m_current_frame]->getAllocation(), &mapped);
    if (mr == VK_SUCCESS && mapped) {
      const size_t bytes = static_cast<size_t>(offscreen_extent.width) *
                           offscreen_extent.height * 4u;
      if (m_readback_pixels.size() < bytes) {
        m_readback_pixels.resize(bytes);
      }
      std::memcpy(m_readback_pixels.data(), mapped, bytes);
      vmaUnmapMemory(vkAlloc(this)->getAllocator(),
                     m_readback_staging[m_current_frame]->getAllocation());

      m_viewport_presenter->presentCpuRgba8(
          m_readback_pixels.data(), offscreen_extent.width,
          offscreen_extent.height, offscreen_extent.width * 4u);
    }
  }

  if (m_renderdoc_capture) {
    m_renderdoc_capture->endFrame(instance);
  }

  m_current_frame = (m_current_frame + 1) % VulkanSync::k_max_frames_in_flight;
}

void RenderSystem::onEvent(Event& event) {
  if (m_renderdoc_capture && m_renderdoc_capture->isAttached() &&
      event.getEventType() == EventType::KeyPressed) {
    auto& key_event = static_cast<KeyPressedEvent&>(event);
    if (!key_event.isRepeat() && key_event.getKeyCode() == SDLK_F11) {
      m_renderdoc_capture->triggerCapture();
    }
  }

  if (m_editor_camera) {
    m_editor_camera->onEvent(event);
  }
}

}  // namespace Blunder
