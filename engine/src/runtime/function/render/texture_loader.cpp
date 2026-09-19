#include "runtime/function/render/texture_loader.h"

#include <atomic>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

#include <stb_image.h>
#include <vulkan/vulkan.h>

#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/job/job_system.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_sync.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

namespace {

constexpr uint32_t k_cpu_pending = 0;
constexpr uint32_t k_cpu_done = 1;
constexpr uint32_t k_cpu_failed = 2;
constexpr VkDeviceSize k_staging_byte_cap = 256ull * 1024ull * 1024ull;
constexpr VkDeviceSize k_min_staging_capacity = 64ull * 1024ull;

struct RequestRecord {
  eastl::string key;
  uint64_t request_generation{0};
  std::atomic<uint32_t> cpu_state{k_cpu_pending};
  std::filesystem::path absolute_path;
  std::vector<uint8_t> pixels;
  uint32_t width{0};
  uint32_t height{0};
  bool gpu_submitted{false};
  eastl::unique_ptr<VulkanTexture> uploading;
  uint64_t timeline_value{0};
  VkCommandBuffer command_buffer{VK_NULL_HANDLE};
  uint32_t staging_index{UINT32_MAX};
};

struct StagingSlot {
  eastl::unique_ptr<VulkanBuffer> buffer;
  VkDeviceSize capacity{0};
  bool in_use{false};
};

VkDeviceSize alignedStagingCapacity(VkDeviceSize size) {
  if (size < k_min_staging_capacity) {
    return k_min_staging_capacity;
  }
  return size;
}

}  // namespace

struct TextureLoaderImpl {
  JobSystem* job_system{nullptr};
  VulkanContext* context{nullptr};
  VulkanAllocator* allocator{nullptr};
  uint64_t generation{1};
  bool accepting{true};
  uint32_t submitted_job_count{0};
  uint32_t gpu_enqueue_count{0};
  bool residency_changed{false};
  VkDeviceSize outstanding_staging_bytes{0};
  eastl::unordered_map<eastl::string, eastl::unique_ptr<RequestRecord>> requests;
  eastl::vector<StagingSlot> staging;
};

namespace {

void decodeJob(void* job_data) {
  RequestRecord* record = static_cast<RequestRecord*>(job_data);
  std::ifstream stream(record->absolute_path,
                       std::ios::binary | std::ios::ate);
  if (!stream.is_open()) {
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return;
  }
  const std::streamsize size = stream.tellg();
  if (size <= 0) {
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return;
  }
  stream.seekg(0, std::ios::beg);
  std::vector<uint8_t> file_bytes(static_cast<size_t>(size));
  if (!stream.read(reinterpret_cast<char*>(file_bytes.data()), size)) {
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return;
  }
  if (file_bytes.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* decoded = stbi_load_from_memory(
      file_bytes.data(), static_cast<int>(file_bytes.size()), &width, &height,
      &channels, STBI_rgb_alpha);
  if (decoded == nullptr || width <= 0 || height <= 0) {
    if (decoded != nullptr) {
      stbi_image_free(decoded);
    }
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return;
  }

  const size_t pixel_bytes =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  record->pixels.resize(pixel_bytes);
  std::memcpy(record->pixels.data(), decoded, pixel_bytes);
  stbi_image_free(decoded);
  record->width = static_cast<uint32_t>(width);
  record->height = static_cast<uint32_t>(height);
  record->cpu_state.store(k_cpu_done, std::memory_order_release);
}

int32_t rentStaging(TextureLoaderImpl& impl, VkDeviceSize byte_size) {
  for (uint32_t i = 0; i < impl.staging.size(); ++i) {
    StagingSlot& slot = impl.staging[i];
    if (!slot.in_use && slot.buffer && slot.capacity >= byte_size) {
      slot.in_use = true;
      impl.outstanding_staging_bytes += slot.capacity;
      return static_cast<int32_t>(i);
    }
  }
  const VkDeviceSize capacity = alignedStagingCapacity(byte_size);
  if (impl.outstanding_staging_bytes + capacity > k_staging_byte_cap) {
    return -1;
  }
  StagingSlot slot{};
  slot.buffer = eastl::make_unique<VulkanBuffer>();
  slot.buffer->create(impl.allocator, capacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
  slot.capacity = capacity;
  slot.in_use = true;
  impl.outstanding_staging_bytes += capacity;
  impl.staging.push_back(eastl::move(slot));
  return static_cast<int32_t>(impl.staging.size() - 1);
}

void returnStaging(TextureLoaderImpl& impl, uint32_t index) {
  if (index >= impl.staging.size()) {
    return;
  }
  StagingSlot& slot = impl.staging[index];
  if (!slot.in_use) {
    return;
  }
  slot.in_use = false;
  if (impl.outstanding_staging_bytes >= slot.capacity) {
    impl.outstanding_staging_bytes -= slot.capacity;
  } else {
    impl.outstanding_staging_bytes = 0;
  }
}

void recycleGpu(TextureLoaderImpl& impl, RequestRecord& record) {
  if (impl.context == nullptr) {
    return;
  }
  if (record.command_buffer != VK_NULL_HANDLE) {
    impl.context->freeImmediateCommandBuffer(record.command_buffer);
    record.command_buffer = VK_NULL_HANDLE;
  }
  record.timeline_value = 0;
  if (record.staging_index != UINT32_MAX) {
    returnStaging(impl, record.staging_index);
    record.staging_index = UINT32_MAX;
  }
}

void dropCpuPayload(RequestRecord& record) {
  record.pixels.clear();
  record.pixels.shrink_to_fit();
  record.width = 0;
  record.height = 0;
}

void finishRecord(TextureLoaderImpl& impl, const eastl::string& key) {
  auto it = impl.requests.find(key);
  if (it == impl.requests.end()) {
    return;
  }
  if (impl.context != nullptr) {
    impl.context->setAsyncTextureUploadPending(key, false);
  }
  impl.requests.erase(it);
}

void enqueueGpu(TextureLoaderImpl& impl, RequestRecord& record) {
  if (impl.context == nullptr || impl.allocator == nullptr) {
    dropCpuPayload(record);
    finishRecord(impl, record.key);
    return;
  }
  if (record.request_generation != impl.generation) {
    dropCpuPayload(record);
    finishRecord(impl, record.key);
    return;
  }

  if (record.pixels.empty() || record.width == 0 || record.height == 0) {
    dropCpuPayload(record);
    finishRecord(impl, record.key);
    return;
  }

  const size_t expected_bytes = static_cast<size_t>(record.width) *
                                static_cast<size_t>(record.height) * 4u;
  if (record.pixels.size() != expected_bytes) {
    LOG_ERROR("[TextureLoader] pixel byte size {} != {}x{}x4",
              record.pixels.size(), record.width, record.height);
    dropCpuPayload(record);
    finishRecord(impl, record.key);
    return;
  }

  const VkDeviceSize byte_size =
      static_cast<VkDeviceSize>(record.pixels.size());
  if (byte_size > k_staging_byte_cap) {
    LOG_ERROR("[TextureLoader] decoded image {} bytes exceeds staging cap {}",
              static_cast<uint64_t>(byte_size),
              static_cast<uint64_t>(k_staging_byte_cap));
    dropCpuPayload(record);
    finishRecord(impl, record.key);
    return;
  }
  const int32_t staging_index = rentStaging(impl, byte_size);
  if (staging_index < 0) {
    return;
  }

  StagingSlot& slot = impl.staging[static_cast<uint32_t>(staging_index)];
  slot.buffer->upload(record.pixels.data(), byte_size);

  auto texture = eastl::make_unique<VulkanTexture>();
  texture->createEmpty(impl.context, impl.allocator, record.width,
                       record.height);

  VkCommandBuffer command_buffer = impl.context->beginImmediateCommands();
  texture->image().recordCopyFromBuffer(command_buffer, slot.buffer->getBuffer(),
                                        record.width, record.height);
  record.timeline_value =
      impl.context->submitImmediateCommandsNoWait(command_buffer);

  record.uploading = eastl::move(texture);
  record.command_buffer = command_buffer;
  record.staging_index = static_cast<uint32_t>(staging_index);
  record.gpu_submitted = true;
  ++impl.gpu_enqueue_count;
  dropCpuPayload(record);
}

void completeGpu(TextureLoaderImpl& impl, RequestRecord& record) {
  const bool stale = record.request_generation != impl.generation;
  const eastl::string key = record.key;
  eastl::unique_ptr<VulkanTexture> texture = eastl::move(record.uploading);
  recycleGpu(impl, record);

  if (!stale && texture && impl.context != nullptr) {
    VulkanTexture* resident = texture.get();
    impl.context->adoptUploadedTexture(key, eastl::move(texture));
    impl.context->bindlessTextureTable().acquire(resident);
    impl.residency_changed = true;
  } else if (texture) {
    texture->destroy();
    texture.reset();
  }

  finishRecord(impl, key);
}

void abandonGpu(TextureLoaderImpl& impl, RequestRecord& record) {
  record.uploading.reset();
  recycleGpu(impl, record);
  dropCpuPayload(record);
  finishRecord(impl, record.key);
}

void pollFences(TextureLoaderImpl& impl) {
  if (impl.context == nullptr) {
    return;
  }
  eastl::vector<eastl::string> completed;
  for (auto& [key, record] : impl.requests) {
    if (!record || !record->gpu_submitted || record->timeline_value == 0 ||
        impl.context->sync() == nullptr) {
      continue;
    }
    if (!impl.context->sync()->valueReached(record->timeline_value)) {
      continue;
    }
    completed.push_back(key);
  }
  for (const eastl::string& key : completed) {
    auto it = impl.requests.find(key);
    if (it != impl.requests.end() && it->second) {
      completeGpu(impl, *it->second);
    }
  }
}

void pollCpu(TextureLoaderImpl& impl) {
  eastl::vector<eastl::string> ready;
  eastl::vector<eastl::string> drop;
  for (auto& [key, record] : impl.requests) {
    if (!record || record->gpu_submitted) {
      continue;
    }
    const uint32_t state = record->cpu_state.load(std::memory_order_acquire);
    if (state == k_cpu_pending) {
      continue;
    }
    if (state == k_cpu_failed || record->request_generation != impl.generation) {
      if (state == k_cpu_failed) {
        LOG_ERROR("[TextureLoader] CPU decode failed for {}", key.c_str());
      }
      drop.push_back(key);
      continue;
    }
    ready.push_back(key);
  }
  for (const eastl::string& key : drop) {
    auto it = impl.requests.find(key);
    if (it != impl.requests.end() && it->second) {
      dropCpuPayload(*it->second);
    }
    finishRecord(impl, key);
  }
  for (const eastl::string& key : ready) {
    auto it = impl.requests.find(key);
    if (it != impl.requests.end() && it->second) {
      enqueueGpu(impl, *it->second);
    }
  }
}

RequestRecord* beginRequest(TextureLoaderImpl& impl, const eastl::string& key) {
  if (!impl.accepting || key.empty()) {
    return nullptr;
  }
  if (impl.context != nullptr && impl.context->findUploadedTexture(key) != nullptr) {
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
  if (impl.context != nullptr) {
    impl.context->setAsyncTextureUploadPending(key, true);
  }
  return raw;
}

}  // namespace

TextureLoader::TextureLoader() = default;

TextureLoader::~TextureLoader() { shutdown(); }

void TextureLoader::initialize(const InitInfo& info) {
  shutdown();
  m_impl = eastl::make_unique<TextureLoaderImpl>();
  m_impl->job_system = info.job_system;
  if (info.context != nullptr && info.allocator != nullptr &&
      info.context->getDevice() != VK_NULL_HANDLE) {
    m_impl->context = info.context;
    m_impl->allocator = info.allocator;
  }
  m_impl->accepting = true;
}

void TextureLoader::shutdown() {
  if (!m_impl) {
    return;
  }
  stopAndWaitCpu();
  for (auto& [key, record] : m_impl->requests) {
    if (!record) {
      continue;
    }
    if (m_impl->context != nullptr) {
      m_impl->context->setAsyncTextureUploadPending(record->key, false);
      if (record->command_buffer != VK_NULL_HANDLE) {
        m_impl->context->freeImmediateCommandBuffer(record->command_buffer);
        record->command_buffer = VK_NULL_HANDLE;
      }
      record->timeline_value = 0;
    }
    record->uploading.reset();
    record->pixels.clear();
  }
  m_impl->requests.clear();
  for (StagingSlot& slot : m_impl->staging) {
    if (slot.buffer) {
      slot.buffer->destroy();
      slot.buffer.reset();
    }
  }
  m_impl->staging.clear();
  m_impl->outstanding_staging_bytes = 0;
  m_impl.reset();
}

void TextureLoader::stopAndWaitCpu() {
  if (!m_impl) {
    return;
  }
  m_impl->accepting = false;
  if (m_impl->job_system != nullptr && m_impl->job_system->isInitialized()) {
    m_impl->job_system->wait();
  }
}

void TextureLoader::dropScene() {
  if (!m_impl) {
    return;
  }
  ++m_impl->generation;
}

uint32_t TextureLoader::submittedJobCount() const {
  return m_impl ? m_impl->submitted_job_count : 0;
}

uint32_t TextureLoader::gpuEnqueueCount() const {
  return m_impl ? m_impl->gpu_enqueue_count : 0;
}

uint32_t TextureLoader::inFlightCount() const {
  return m_impl ? static_cast<uint32_t>(m_impl->requests.size()) : 0;
}

size_t TextureLoader::decodedByteSize(const eastl::string& key) const {
  if (!m_impl) {
    return 0;
  }
  auto it = m_impl->requests.find(key);
  if (it == m_impl->requests.end() || !it->second) {
    return 0;
  }
  const RequestRecord& record = *it->second;
  if (record.cpu_state.load(std::memory_order_acquire) != k_cpu_done) {
    return 0;
  }
  return record.pixels.size();
}

uint64_t TextureLoader::generation() const {
  return m_impl ? m_impl->generation : 0;
}

bool TextureLoader::isGpuEnabled() const {
  return m_impl != nullptr && m_impl->context != nullptr &&
         m_impl->allocator != nullptr;
}

void TextureLoader::tick() {
  if (!m_impl) {
    return;
  }
  pollFences(*m_impl);
  pollCpu(*m_impl);
}

bool TextureLoader::consumeResidencyChanged() {
  if (!m_impl) {
    return false;
  }
  const bool changed = m_impl->residency_changed;
  m_impl->residency_changed = false;
  return changed;
}

VulkanTexture* TextureLoader::request(const Texture2DAsset* asset) {
  if (!m_impl || asset == nullptr) {
    return nullptr;
  }
  const eastl::string key = gpuTextureCacheKey(*asset);
  if (m_impl->context != nullptr) {
    if (VulkanTexture* resident = m_impl->context->findUploadedTexture(key)) {
      return resident;
    }
  }
  RequestRecord* record = beginRequest(*m_impl, key);
  if (record == nullptr) {
    if (m_impl->context != nullptr) {
      return m_impl->context->findUploadedTexture(key);
    }
    return nullptr;
  }

  if (asset->getPixelData() != nullptr && asset->getPixelByteSize() > 0 &&
      asset->getWidth() > 0 && asset->getHeight() > 0) {
    record->width = asset->getWidth();
    record->height = asset->getHeight();
    record->pixels.assign(asset->getPixelData(),
                          asset->getPixelData() + asset->getPixelByteSize());
    record->cpu_state.store(k_cpu_done, std::memory_order_release);
    return nullptr;
  }

  if (asset->getAbsolutePath().empty() || m_impl->job_system == nullptr ||
      !m_impl->job_system->isInitialized()) {
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return nullptr;
  }
  record->absolute_path = asset->getAbsolutePath();
  ++m_impl->submitted_job_count;
  m_impl->job_system->submit(&decodeJob, record);
  return nullptr;
}

void TextureLoader::requestFile(const eastl::string& key,
                                const std::filesystem::path& absolute_path) {
  if (!m_impl) {
    return;
  }
  RequestRecord* record = beginRequest(*m_impl, key);
  if (record == nullptr) {
    return;
  }
  if (absolute_path.empty() || m_impl->job_system == nullptr ||
      !m_impl->job_system->isInitialized()) {
    record->cpu_state.store(k_cpu_failed, std::memory_order_release);
    return;
  }
  record->absolute_path = absolute_path;
  ++m_impl->submitted_job_count;
  m_impl->job_system->submit(&decodeJob, record);
}

}  // namespace Blunder
