#include "runtime/function/render/mesh_loader.h"

#include <atomic>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#include <cgltf.h>
#include <glm/vec3.hpp>

#include "EASTL/unordered_map.h"
#include "EASTL/unordered_set.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/job/job_system.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_manager/asset_manager.h"

namespace Blunder {

namespace {

constexpr uint32_t k_cpu_pending = 0;
constexpr uint32_t k_cpu_done = 1;
constexpr uint32_t k_cpu_failed = 2;

struct RequestRecord {
  eastl::string key;
  eastl::string virtual_path;
  eastl::string guid;
  uint64_t request_generation{0};
  std::atomic<uint32_t> cpu_state{k_cpu_pending};
  std::filesystem::path cooked_path;
  std::filesystem::path source_path;
  std::filesystem::path descriptor_path;
  eastl::vector<MeshVertex> vertices;
  eastl::vector<uint32_t> indices;
  MeshSkinData skin_data;
  MeshletPayload meshlets;
};

bool readFirstGltfPrimitive(const std::filesystem::path& absolute,
                            eastl::vector<MeshVertex>& out_vertices,
                            eastl::vector<uint32_t>& out_indices) {
  std::ifstream stream(absolute, std::ios::binary | std::ios::ate);
  if (!stream.is_open()) {
    return false;
  }
  const std::streamsize size = stream.tellg();
  if (size <= 0) {
    return false;
  }
  stream.seekg(0, std::ios::beg);
  eastl::vector<uint8_t> bytes(static_cast<size_t>(size));
  if (!stream.read(reinterpret_cast<char*>(bytes.data()), size)) {
    return false;
  }

  cgltf_options options{};
  cgltf_data* data = nullptr;
  const cgltf_result parse_result =
      cgltf_parse(&options, bytes.data(), bytes.size(), &data);
  if (parse_result != cgltf_result_success || data == nullptr) {
    return false;
  }

  const std::string absolute_string = absolute.string();
  const cgltf_result buffer_result =
      cgltf_load_buffers(&options, data, absolute_string.c_str());
  if (buffer_result != cgltf_result_success) {
    cgltf_free(data);
    return false;
  }

  const cgltf_primitive* primitive = nullptr;
  for (cgltf_size mesh_index = 0; mesh_index < data->meshes_count && primitive == nullptr;
       ++mesh_index) {
    const cgltf_mesh& mesh = data->meshes[mesh_index];
    for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
         ++primitive_index) {
      if (mesh.primitives[primitive_index].type == cgltf_primitive_type_triangles) {
        primitive = &mesh.primitives[primitive_index];
        break;
      }
    }
  }
  if (primitive == nullptr) {
    cgltf_free(data);
    return false;
  }

  const cgltf_attribute* position_attribute = nullptr;
  for (cgltf_size i = 0; i < primitive->attributes_count; ++i) {
    if (primitive->attributes[i].type == cgltf_attribute_type_position) {
      position_attribute = &primitive->attributes[i];
      break;
    }
  }
  if (position_attribute == nullptr || position_attribute->data == nullptr) {
    cgltf_free(data);
    return false;
  }

  const cgltf_accessor* position_accessor = position_attribute->data;
  const size_t vertex_count = static_cast<size_t>(position_accessor->count);
  if (vertex_count == 0) {
    cgltf_free(data);
    return false;
  }

  out_vertices.resize(vertex_count);
  for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    if (!cgltf_accessor_read_float(position_accessor, vertex_index, position, 3)) {
      cgltf_free(data);
      return false;
    }
    out_vertices[vertex_index].position = transformPointGltfToEngine(
        glm::vec3(position[0], position[1], position[2]));
  }

  if (primitive->indices != nullptr) {
    const size_t index_count = static_cast<size_t>(primitive->indices->count);
    out_indices.resize(index_count);
    for (size_t index = 0; index < index_count; ++index) {
      const cgltf_size value =
          cgltf_accessor_read_index(primitive->indices, index);
      if (value > std::numeric_limits<uint32_t>::max()) {
        cgltf_free(data);
        return false;
      }
      out_indices[index] = static_cast<uint32_t>(value);
    }
  } else {
    out_indices.resize(vertex_count);
    for (size_t index = 0; index < vertex_count; ++index) {
      out_indices[index] = static_cast<uint32_t>(index);
    }
  }

  cgltf_free(data);
  return !out_vertices.empty() && !out_indices.empty();
}

void cpuReadJob(void* job_data) {
  RequestRecord* record = static_cast<RequestRecord*>(job_data);
  eastl::vector<MeshVertex> vertices;
  eastl::vector<uint32_t> indices;
  MeshSkinData skin_data;
  MeshletPayload meshlets;
  bool ok = false;
  if (!record->cooked_path.empty() &&
      readMeshCookFile(record->cooked_path, vertices, indices, &skin_data,
                       &meshlets)) {
    ok = !vertices.empty() && !indices.empty();
  }
  if (!ok && !record->source_path.empty()) {
    vertices.clear();
    indices.clear();
    skin_data = {};
    meshlets = {};
    ok = readFirstGltfPrimitive(record->source_path, vertices, indices);
  }
  if (!ok) {
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return;
  }
  record->vertices = eastl::move(vertices);
  record->indices = eastl::move(indices);
  record->skin_data = eastl::move(skin_data);
  record->meshlets = eastl::move(meshlets);
  record->cpu_state.store(k_cpu_done, std::memory_order_release);
}

}  // namespace

struct MeshLoaderImpl {
  JobSystem* job_system{nullptr};
  AssetManager* asset_manager{nullptr};
  uint64_t generation{1};
  bool accepting{true};
  bool gpu_enabled{false};
  uint32_t gpu_budget{MeshLoader::kDefaultGpuMeshBudget};
  uint32_t submitted_job_count{0};
  uint32_t gpu_enqueue_count{0};
  bool residency_changed{false};
  eastl::unordered_map<eastl::string, eastl::unique_ptr<RequestRecord>> requests;
  eastl::unordered_map<eastl::string, MeshLoader::Request> request_args;
  eastl::unordered_map<eastl::string, eastl::shared_ptr<MeshAsset>> cpu_meshes;
  eastl::unordered_set<eastl::string> failed_keys;
  eastl::unordered_set<eastl::string> gpu_uploaded;
  eastl::vector<eastl::string> submit_order;
  eastl::vector<eastl::string> gpu_pending;
};

namespace {

void publishCpuMesh(MeshLoaderImpl& impl, RequestRecord& record) {
  if (record.vertices.empty() || record.indices.empty()) {
    impl.failed_keys.insert(record.key);
    return;
  }
  if (impl.cpu_meshes.find(record.key) != impl.cpu_meshes.end()) {
    return;
  }

  Asset::Meta meta;
  meta.virtual_path = record.virtual_path.empty() ? record.key : record.virtual_path;
  meta.absolute_path = record.descriptor_path;
  eastl::shared_ptr<MeshAsset> mesh;
  if (record.skin_data.isValid()) {
    mesh = eastl::make_shared<MeshAsset>(
        eastl::move(meta), eastl::move(record.vertices),
        eastl::move(record.indices), AssetHandle{}, nullptr,
        eastl::move(record.skin_data), true);
  } else {
    mesh = eastl::make_shared<MeshAsset>(
        eastl::move(meta), eastl::move(record.vertices),
        eastl::move(record.indices), AssetHandle{}, nullptr, MeshSkinData{},
        true, eastl::move(record.meshlets));
  }
  if (impl.asset_manager != nullptr) {
    impl.asset_manager->adoptStreamedMesh(mesh, record.guid);
  }
  impl.cpu_meshes[record.key] = mesh;
  if (impl.gpu_uploaded.find(record.key) == impl.gpu_uploaded.end()) {
    bool already_pending = false;
    for (const eastl::string& pending : impl.gpu_pending) {
      if (pending == record.key) {
        already_pending = true;
        break;
      }
    }
    if (!already_pending) {
      impl.gpu_pending.push_back(record.key);
    }
  }
  impl.residency_changed = true;
}

void pollCpu(MeshLoaderImpl& impl) {
  eastl::vector<eastl::string> finished;
  for (auto& [key, record] : impl.requests) {
    if (!record) {
      continue;
    }
    const uint32_t state = record->cpu_state.load(std::memory_order_acquire);
    if (state == k_cpu_pending) {
      continue;
    }
    if (state == k_cpu_failed || record->request_generation != impl.generation) {
      if (state == k_cpu_failed &&
          record->request_generation == impl.generation) {
        LOG_ERROR("[MeshLoader] CPU read failed for {}", key.c_str());
        impl.failed_keys.insert(key);
      }
      record->vertices.clear();
      record->indices.clear();
      finished.push_back(key);
      continue;
    }
    publishCpuMesh(impl, *record);
    finished.push_back(key);
  }
  for (const eastl::string& key : finished) {
    impl.requests.erase(key);
  }
}

RequestRecord* beginRequest(MeshLoaderImpl& impl, const eastl::string& key) {
  if (!impl.accepting || key.empty()) {
    return nullptr;
  }
  if (impl.cpu_meshes.find(key) != impl.cpu_meshes.end()) {
    if (impl.gpu_uploaded.find(key) == impl.gpu_uploaded.end()) {
      bool already_pending = false;
      for (const eastl::string& pending : impl.gpu_pending) {
        if (pending == key) {
          already_pending = true;
          break;
        }
      }
      if (!already_pending) {
        impl.gpu_pending.push_back(key);
      }
    }
    return nullptr;
  }
  auto it = impl.requests.find(key);
  if (it != impl.requests.end() && it->second) {
    it->second->request_generation = impl.generation;
    return nullptr;
  }
  auto record = eastl::make_unique<RequestRecord>();
  record->key = key;
  record->request_generation = impl.generation;
  RequestRecord* raw = record.get();
  impl.requests[key] = eastl::move(record);
  impl.submit_order.push_back(key);
  return raw;
}

}  // namespace

MeshLoader::MeshLoader() = default;

MeshLoader::~MeshLoader() { shutdown(); }

void MeshLoader::initialize(const InitInfo& info) {
  shutdown();
  m_impl = eastl::make_unique<MeshLoaderImpl>();
  m_impl->job_system = info.job_system;
  m_impl->asset_manager = info.asset_manager;
  if (info.gpu_budget > 0) {
    m_impl->gpu_budget = info.gpu_budget;
  }
  m_impl->accepting = true;
}

void MeshLoader::shutdown() {
  if (!m_impl) {
    return;
  }
  stopAndWaitCpu();
  m_impl->requests.clear();
  m_impl->request_args.clear();
  m_impl->cpu_meshes.clear();
  m_impl->failed_keys.clear();
  m_impl->gpu_uploaded.clear();
  m_impl->submit_order.clear();
  m_impl->gpu_pending.clear();
  m_impl.reset();
}

void MeshLoader::stopAndWaitCpu() {
  if (!m_impl) {
    return;
  }
  m_impl->accepting = false;
  if (m_impl->job_system != nullptr && m_impl->job_system->isInitialized()) {
    m_impl->job_system->wait();
  }
}

void MeshLoader::dropScene() {
  if (!m_impl) {
    return;
  }
  ++m_impl->generation;
  m_impl->failed_keys.clear();
}

void MeshLoader::enableGpu(bool enabled) {
  if (!m_impl) {
    return;
  }
  m_impl->gpu_enabled = enabled;
}

void MeshLoader::tick() {
  if (!m_impl) {
    return;
  }
  pollCpu(*m_impl);
}

void MeshLoader::requeue(const eastl::string& key) {
  if (!m_impl || key.empty()) {
    return;
  }
  auto it = m_impl->request_args.find(key);
  if (it == m_impl->request_args.end()) {
    return;
  }
  m_impl->failed_keys.erase(key);
  request(it->second);
}

void MeshLoader::request(const Request& request) {
  if (!m_impl || request.key.empty()) {
    return;
  }
  m_impl->request_args[request.key] = request;
  RequestRecord* record = beginRequest(*m_impl, request.key);
  if (record == nullptr) {
    return;
  }
  record->virtual_path = request.virtual_path;
  record->guid = request.guid;
  record->cooked_path = request.cooked_path;
  record->source_path = request.source_path;
  record->descriptor_path = request.descriptor_path;
  if (m_impl->job_system == nullptr || !m_impl->job_system->isInitialized()) {
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return;
  }
  ++m_impl->submitted_job_count;
  m_impl->job_system->submit(&cpuReadJob, record);
}

eastl::shared_ptr<MeshAsset> MeshLoader::cpuMesh(const eastl::string& key) const {
  if (!m_impl || key.empty()) {
    return nullptr;
  }
  auto it = m_impl->cpu_meshes.find(key);
  if (it == m_impl->cpu_meshes.end()) {
    return nullptr;
  }
  return it->second;
}

bool MeshLoader::isFailed(const eastl::string& key) const {
  if (!m_impl) {
    return false;
  }
  return m_impl->failed_keys.find(key) != m_impl->failed_keys.end();
}

bool MeshLoader::isGpuUploaded(const eastl::string& key) const {
  if (!m_impl) {
    return false;
  }
  return m_impl->gpu_uploaded.find(key) != m_impl->gpu_uploaded.end();
}

eastl::vector<eastl::string> MeshLoader::gpuPendingKeys() const {
  if (!m_impl) {
    return {};
  }
  return m_impl->gpu_pending;
}

void MeshLoader::markGpuUploaded(const eastl::string& key) {
  if (!m_impl || key.empty()) {
    return;
  }
  m_impl->gpu_uploaded.insert(key);
  for (auto it = m_impl->gpu_pending.begin(); it != m_impl->gpu_pending.end();
       ++it) {
    if (*it == key) {
      m_impl->gpu_pending.erase(it);
      break;
    }
  }
  ++m_impl->gpu_enqueue_count;
  m_impl->residency_changed = true;
}

void MeshLoader::markGpuFailed(const eastl::string& key) {
  if (!m_impl || key.empty()) {
    return;
  }
  m_impl->failed_keys.insert(key);
  for (auto it = m_impl->gpu_pending.begin(); it != m_impl->gpu_pending.end();
       ++it) {
    if (*it == key) {
      m_impl->gpu_pending.erase(it);
      break;
    }
  }
}

uint32_t MeshLoader::submittedJobCount() const {
  return m_impl ? m_impl->submitted_job_count : 0;
}

uint32_t MeshLoader::gpuEnqueueCount() const {
  return m_impl ? m_impl->gpu_enqueue_count : 0;
}

uint32_t MeshLoader::inFlightCount() const {
  return m_impl ? static_cast<uint32_t>(m_impl->requests.size()) : 0;
}

uint32_t MeshLoader::gpuBudget() const {
  return m_impl ? m_impl->gpu_budget : kDefaultGpuMeshBudget;
}

uint64_t MeshLoader::generation() const {
  return m_impl ? m_impl->generation : 0;
}

bool MeshLoader::isGpuEnabled() const {
  return m_impl != nullptr && m_impl->gpu_enabled;
}

eastl::vector<eastl::string> MeshLoader::queuedKeys() const {
  if (!m_impl) {
    return {};
  }
  return m_impl->submit_order;
}

bool MeshLoader::consumeResidencyChanged() {
  if (!m_impl) {
    return false;
  }
  const bool changed = m_impl->residency_changed;
  m_impl->residency_changed = false;
  return changed;
}

}  // namespace Blunder
