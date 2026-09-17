#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/vector_uint4.hpp>

#include <cgltf.h>

#include "EASTL/shared_ptr.h"

#include "runtime/function/scene/entity_id.h"
#include "runtime/resource/asset/meshlet.h"

namespace Blunder {

class GpuMesh;
class MaterialAsset;
class VulkanTexture;

inline constexpr uint32_t k_max_gpu_driven_instances = 16128u;
inline constexpr uint32_t k_max_gpu_driven_meshlets = 262144u;
inline constexpr uint32_t k_gpu_driven_cpu_receiver_slots = 256u;
inline constexpr uint32_t k_gpu_driven_receiver_slot_count =
    k_gpu_driven_cpu_receiver_slots + k_max_gpu_driven_instances;
inline constexpr uint32_t k_gpu_driven_receiver_slot_mask = 0x3FFFu;
static_assert(k_gpu_driven_receiver_slot_count ==
              k_gpu_driven_receiver_slot_mask + 1u);
inline constexpr uint32_t k_gpu_driven_receiver_unlit_bit = 0x4000u;
inline constexpr uint32_t k_gpu_driven_receiver_two_sided_bit = 0x8000u;
inline constexpr uint32_t k_gpu_driven_flag_two_sided = 1u;
inline constexpr uint32_t k_gpu_driven_flag_skip_cone = 2u;
inline constexpr uint32_t k_gpu_driven_flag_early = 4u;
inline constexpr uint32_t k_gpu_driven_meshlet_batch_shift = 16u;

/// CPU draw submitted to GPU-driven static opaque/alpha-clip.
struct GpuDrivenDraw {
  GpuMesh* gpu_mesh{nullptr};
  eastl::shared_ptr<MaterialAsset> material;
  VulkanTexture* base_color_texture{nullptr};
  VulkanTexture* metallic_roughness_texture{nullptr};
  VulkanTexture* normal_texture{nullptr};
  VulkanTexture* occlusion_texture{nullptr};
  glm::mat4 model{1.0f};
  uint32_t receiver_id{0};
  float alpha_cutoff{0.5f};
  cgltf_alpha_mode alpha_mode{cgltf_alpha_mode_opaque};
  bool double_sided{false};
  EntityId entity_id{k_invalid_entity_id};
};

struct GpuDrivenInstanceGpu {
  glm::mat4 world{1.0f};
  glm::mat4 normal_matrix{1.0f};
  glm::uvec4 bindless_texture_indices{0};
  glm::vec4 base_color_factor{1.0f};
  glm::vec4 metallic_roughness_factors{1.0f, 1.0f, 0.5f, 0.0f};
  glm::vec4 pbr_texture_flags{0.0f};
  glm::vec4 material_flags{0.0f};
  uint32_t receiver_id{0};
  uint32_t flags{0};
  uint32_t meshlet_offset{0};
  uint32_t meshlet_count{0};
  uint32_t light_mask{0xFFFFFFFFu};
  uint32_t pad0{0};
  uint32_t pad1{0};
  uint32_t pad2{0};
};

struct GpuDrivenMeshletGpu {
  glm::vec4 sphere{0.0f};
  glm::vec4 cone{0.0f, 0.0f, 1.0f, 1.0f};
  uint32_t instance_index{0};
  uint32_t first_index{0};
  uint32_t index_count{0};
  uint32_t flags{0};
  uint32_t vertex_offset{0};
  uint32_t vertex_count{0};
  uint32_t triangle_offset{0};
  uint32_t triangle_count{0};
};

struct GpuDrivenCullUniforms {
  glm::mat4 view_projection{1.0f};
  glm::vec4 camera_position{0.0f};
  glm::vec4 frustum_planes[6]{};
  uint32_t meshlet_count{0};
  uint32_t hiz_enabled{0};
  uint32_t hiz_mip_count{0};
  uint32_t skip_cone{0};
  uint32_t compact_commands{0};
  uint32_t pad0{0};
  uint32_t pad1{0};
  uint32_t pad2{0};
  glm::vec4 hiz_size{0.0f};
};

/// std430 / SSBO stride 32. First 20 bytes match VkDrawIndexedIndirectCommand.
struct DrawIndexedIndirectCommand {
  uint32_t index_count{0};
  uint32_t instance_count{0};
  uint32_t first_index{0};
  uint32_t vertex_offset{0};
  uint32_t first_instance{0};
  uint32_t pad0{0};
  uint32_t pad1{0};
  uint32_t pad2{0};
};

static_assert(sizeof(DrawIndexedIndirectCommand) == 32,
              "indirect command SSBO stride must be 32");
static_assert(sizeof(GpuDrivenMeshletGpu) == 64,
              "meshlet SSBO stride must be 64");
static_assert(sizeof(GpuDrivenInstanceGpu) == 240,
              "instance SSBO stride must stay 16-byte aligned");

struct GpuMeshletGpuRecord {
  glm::vec4 sphere{0.0f};
  glm::vec4 cone{0.0f, 0.0f, 1.0f, 1.0f};
  uint32_t first_index{0};
  uint32_t index_count{0};
  uint32_t vertex_offset{0};
  uint32_t vertex_count{0};
  uint32_t triangle_offset{0};
  uint32_t triangle_count{0};
  uint32_t pad[2]{0, 0};
};

inline GpuMeshletGpuRecord packMeshletGpuRecord(const MeshletRecord& src,
                                                uint32_t first_index,
                                                uint32_t index_count) {
  GpuMeshletGpuRecord dst{};
  dst.sphere = glm::vec4(src.center[0], src.center[1], src.center[2], src.radius);
  dst.cone = glm::vec4(static_cast<float>(src.cone_axis[0]) / 127.0f,
                       static_cast<float>(src.cone_axis[1]) / 127.0f,
                       static_cast<float>(src.cone_axis[2]) / 127.0f,
                       static_cast<float>(src.cone_cutoff) / 127.0f);
  dst.first_index = first_index;
  dst.index_count = index_count;
  dst.vertex_offset = src.vertex_offset;
  dst.vertex_count = src.vertex_count;
  dst.triangle_offset = src.triangle_offset;
  dst.triangle_count = src.triangle_count;
  return dst;
}

}  // namespace Blunder
