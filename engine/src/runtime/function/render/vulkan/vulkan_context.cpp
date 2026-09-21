#include "runtime/function/render/vulkan/vulkan_context.h"

#include <cstring>

#include <SDL3/SDL.h>

#include "EASTL/set.h"
#include "EASTL/unique_ptr.h"
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/engine_gpu_cache.h"
#include "runtime/function/render/vrs/vrs_rate.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/resource/asset/texture2d_asset.h"

#include <vk_mem_alloc.h>

namespace Blunder {

namespace {

const char* k_validation_layer_name = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  (void)message_type;
  (void)user_data;

  if (!callback_data || !callback_data->pMessage) {
    return VK_FALSE;
  }

  if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    LOG_ERROR("[VulkanContext::Validation] {}", callback_data->pMessage);
  } else {
    LOG_WARN("[VulkanContext::Validation] {}", callback_data->pMessage);
  }

  return VK_FALSE;
}

bool hasLayer(const char* name) {
  uint32_t layer_count = 0;

  // 列出所有可用的层
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  eastl::vector<VkLayerProperties> layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

  for (const VkLayerProperties& layer : layers) {
    if (std::strcmp(layer.layerName, name) == 0) {
      return true;
    }
  }

  return false;
}

/// <summary>
/// 检查 Vulkan 实例是否支持特定的实例扩展
/// </summary>
/// <param name="name"></param>
/// <returns></returns>
bool hasInstanceExtension(const char* name) {
  uint32_t extension_count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

  eastl::vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                         extensions.data());
  // 检查 validationLayers 中的所有层是否存在于 availableLayers 列表中
  for (const VkExtensionProperties& extension : extensions) {
    if (std::strcmp(extension.extensionName, name) == 0) {
      return true;
    }
  }

  return false;
}

/// <summary>
/// 检查物理设备是否支持特定的设备扩展
/// </summary>
/// <param name="physical_device"></param>
/// <param name="name"></param>
/// <returns></returns>
bool hasDeviceExtension(VkPhysicalDevice physical_device, const char* name) {
  uint32_t extension_count = 0;
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                       &extension_count, nullptr);

  std::vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr,
                                       &extension_count, extensions.data());

  for (const VkExtensionProperties& extension : extensions) {
    if (std::strcmp(extension.extensionName, name) == 0) {
      return true;
    }
  }

  return false;
}

bool descriptorIndexingSupportsBindlessTable(VkPhysicalDevice device) {
  if (!hasDeviceExtension(device, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
    return false;
  }

  VkPhysicalDeviceDescriptorIndexingFeatures indexing{};
  indexing.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
  VkPhysicalDeviceVulkan11Features vulkan11{};
  vulkan11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  vulkan11.pNext = &indexing;
  VkPhysicalDeviceFeatures2 features2{};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &vulkan11;
  vkGetPhysicalDeviceFeatures2(device, &features2);
  if (!indexing.runtimeDescriptorArray ||
      !indexing.shaderSampledImageArrayNonUniformIndexing ||
      !indexing.descriptorBindingSampledImageUpdateAfterBind ||
      !indexing.descriptorBindingPartiallyBound ||
      !indexing.descriptorBindingUpdateUnusedWhilePending) {
    return false;
  }

  VkPhysicalDeviceDescriptorIndexingProperties indexing_props{};
  indexing_props.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
  VkPhysicalDeviceProperties2 props2{};
  props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  props2.pNext = &indexing_props;
  vkGetPhysicalDeviceProperties2(device, &props2);
  constexpr uint32_t k_need = BindlessTextureIndexTable::k_capacity;
  return indexing_props.maxPerStageDescriptorUpdateAfterBindSampledImages >=
             k_need &&
         indexing_props.maxPerStageDescriptorUpdateAfterBindSamplers >=
             k_need &&
         indexing_props.maxDescriptorSetUpdateAfterBindSampledImages >=
             k_need &&
         indexing_props.maxDescriptorSetUpdateAfterBindSamplers >= k_need;
}

}  // namespace

void VulkanContext::initialize(const VulkanContextCreateInfo& info) {
  m_window_system = info.window_system;
#ifdef NDEBUG // 非调试模式
  m_enable_validation = false;
#else
  m_enable_validation = info.enable_validation;
#endif
  m_slang_build_tag =
      (info.slang_build_tag != nullptr && info.slang_build_tag[0] != '\0')
          ? info.slang_build_tag
          : "unknown";

  // Headless: no VkSurfaceKHR / swapchain. Windowed: Slint Skia owns HWND
  // present; the engine still renders off-screen.
  LOG_INFO("[VulkanContext::initialize] creating Vulkan instance");
  createInstance();
  LOG_INFO("[VulkanContext::initialize] setting up debug messenger");
  setupDebugMessenger();
  LOG_INFO("[VulkanContext::initialize] selecting physical device");
  selectPhysicalDevice();
  LOG_INFO("[VulkanContext::initialize] creating logical device");
  createLogicalDevice();
  m_bindless_table.initialize(m_device);
  createPipelineCache();

  LOG_INFO("[VulkanContext::initialize] Vulkan context initialized");
}

void VulkanContext::shutdown() {
  LOG_INFO("[VulkanContext::shutdown] tearing down Vulkan context");

  if (m_device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device);
  }

  m_secondary_command_buffers.shutdown();

  flushRetiredSampledImages(true);

  savePipelineCache();
  destroyPipelineCache();
  m_bindless_table.shutdown();

  if (m_device != VK_NULL_HANDLE &&
      m_immediate_command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device, m_immediate_command_pool, nullptr);
    m_immediate_command_pool = VK_NULL_HANDLE;
  }

  if (m_device != VK_NULL_HANDLE) {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }

  // Headless mode never created a window surface; nothing to destroy here.

  if (m_debug_messenger != VK_NULL_HANDLE) {
    auto* destroy_debug_messenger =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroy_debug_messenger) {
      destroy_debug_messenger(m_instance, m_debug_messenger, nullptr);
    }
    m_debug_messenger = VK_NULL_HANDLE;
  }

  if (m_instance != VK_NULL_HANDLE) {
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
  }

  m_physical_device = VK_NULL_HANDLE;
  m_graphics_queue = VK_NULL_HANDLE;
  m_present_queue = VK_NULL_HANDLE;
  m_graphics_queue_family = 0;
  m_present_queue_family = 0;
  m_physical_device_properties = {};
  m_sampler_anisotropy_enabled = false;
  m_window_system = nullptr;
  m_sync = nullptr;
  m_uploaded_textures.clear();
  m_retired_sampled_images.clear();
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    m_in_flight_retire_seq[i] = 0;
  }
}

VulkanTexture* VulkanContext::ensureUploadedTexture(
    VulkanAllocator* allocator, const Texture2DAsset& asset) {
  if (allocator == nullptr || m_device == VK_NULL_HANDLE) {
    return nullptr;
  }
  const eastl::string key = gpuTextureCacheKey(asset);
  if (VulkanTexture* existing = findUploadedTexture(key)) {
    return existing;
  }
  if (m_async_pending_textures.find(key) != m_async_pending_textures.end()) {
    return nullptr;
  }

  auto uploaded_texture = eastl::make_unique<VulkanTexture>();
  uploaded_texture->createFromTexture2DAsset(this, allocator, asset);
  VulkanTexture* uploaded_texture_ptr = uploaded_texture.get();
  m_uploaded_textures[key] = eastl::move(uploaded_texture);
  LOG_INFO("[VulkanContext] texture uploaded {} ({}x{}, {} bytes)",
           key.c_str(), asset.getWidth(), asset.getHeight(),
           asset.getPixelByteSize());
  return uploaded_texture_ptr;
}

VulkanTexture* VulkanContext::findUploadedTexture(const eastl::string& key) const {
  auto it = m_uploaded_textures.find(key);
  if (it == m_uploaded_textures.end()) {
    return nullptr;
  }
  return it->second.get();
}

void VulkanContext::adoptUploadedTexture(
    eastl::string key, eastl::unique_ptr<VulkanTexture> texture) {
  if (key.empty() || !texture) {
    return;
  }
  m_async_pending_textures.erase(key);
  m_uploaded_textures[eastl::move(key)] = eastl::move(texture);
}

void VulkanContext::setAsyncTextureUploadPending(const eastl::string& key,
                                                 bool pending) {
  if (key.empty()) {
    return;
  }
  if (pending) {
    m_async_pending_textures.insert(key);
  } else {
    m_async_pending_textures.erase(key);
  }
}

bool VulkanContext::isAsyncTextureUploadPending(const eastl::string& key) const {
  return m_async_pending_textures.find(key) != m_async_pending_textures.end();
}

void VulkanContext::destroyUploadedTextures() {
  for (auto& [key, texture] : m_uploaded_textures) {
    if (texture) {
      texture->destroy();
    }
  }
  m_uploaded_textures.clear();
  m_async_pending_textures.clear();
}

void VulkanContext::destroyRetiredSampledImage(const RetiredSampledImage& image) {
  if (m_device == VK_NULL_HANDLE) {
    return;
  }
  if (image.sampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_device, image.sampler, nullptr);
  }
  if (image.view != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device, image.view, nullptr);
  }
  if (image.image != VK_NULL_HANDLE && image.allocator != nullptr) {
    vmaDestroyImage(image.allocator->getAllocator(), image.image,
                    static_cast<VmaAllocation>(image.allocation));
  }
}

void VulkanContext::retireSampledImage(VkImage image, VkImageView view,
                                       VkSampler sampler, void* allocation,
                                       VulkanAllocator* allocator) {
  if (image == VK_NULL_HANDLE && view == VK_NULL_HANDLE &&
      sampler == VK_NULL_HANDLE) {
    return;
  }
  RetiredSampledImage retired{};
  retired.image = image;
  retired.view = view;
  retired.sampler = sampler;
  retired.allocation = allocation;
  retired.allocator = allocator;
  for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
    retired.required_seq[i] = m_in_flight_retire_seq[i] + 1;
  }
  m_retired_sampled_images.push_back(retired);
}

void VulkanContext::onInFlightFenceRetired(uint32_t slot) {
  if (slot >= VulkanSync::k_max_frames_in_flight) {
    return;
  }
  ++m_in_flight_retire_seq[slot];
  flushRetiredSampledImages(false);
}

void VulkanContext::flushRetiredSampledImages(bool immediate) {
  eastl::vector<RetiredSampledImage> kept;
  kept.reserve(m_retired_sampled_images.size());
  for (const RetiredSampledImage& image : m_retired_sampled_images) {
    bool ready = immediate;
    if (!ready) {
      ready = true;
      for (uint32_t i = 0; i < VulkanSync::k_max_frames_in_flight; ++i) {
        if (m_in_flight_retire_seq[i] < image.required_seq[i]) {
          ready = false;
          break;
        }
      }
    }
    if (ready) {
      destroyRetiredSampledImage(image);
    } else {
      kept.push_back(image);
    }
  }
  m_retired_sampled_images.swap(kept);
}

VkCommandBuffer VulkanContext::beginImmediateCommands() {
  ASSERT(m_device != VK_NULL_HANDLE);
  ASSERT(m_immediate_command_pool != VK_NULL_HANDLE);

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = m_immediate_command_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer = VK_NULL_HANDLE;
  const VkResult alloc_result = vkAllocateCommandBuffers(
      m_device, &alloc_info, &command_buffer);
  if (alloc_result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanContext::beginImmediateCommands] "
        "vkAllocateCommandBuffers failed: {}",
        static_cast<int>(alloc_result));
  }

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  const VkResult begin_result =
      vkBeginCommandBuffer(command_buffer, &begin_info);
  if (begin_result != VK_SUCCESS) {
    vkFreeCommandBuffers(m_device, m_immediate_command_pool, 1,
                         &command_buffer);
    LOG_FATAL(
        "[VulkanContext::beginImmediateCommands] vkBeginCommandBuffer "
        "failed: {}",
        static_cast<int>(begin_result));
  }

  return command_buffer;
}

void VulkanContext::endImmediateCommands(VkCommandBuffer command_buffer) {
  ASSERT(m_device != VK_NULL_HANDLE);
  ASSERT(m_immediate_command_pool != VK_NULL_HANDLE);
  ASSERT(command_buffer != VK_NULL_HANDLE);
  ASSERT(m_sync != nullptr);

  const VkResult end_result = vkEndCommandBuffer(command_buffer);
  if (end_result != VK_SUCCESS) {
    vkFreeCommandBuffers(m_device, m_immediate_command_pool, 1,
                         &command_buffer);
    LOG_FATAL(
        "[VulkanContext::endImmediateCommands] vkEndCommandBuffer failed: {}",
        static_cast<int>(end_result));
  }

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  const uint64_t value = m_sync->queueSubmit(m_graphics_queue, submit_info);
  const VkResult wait_result = m_sync->waitValue(value, UINT64_MAX);
  if (wait_result != VK_SUCCESS) {
    vkFreeCommandBuffers(m_device, m_immediate_command_pool, 1,
                         &command_buffer);
    LOG_FATAL(
        "[VulkanContext::endImmediateCommands] wait timeline failed: {}",
        static_cast<int>(wait_result));
  }

  vkFreeCommandBuffers(m_device, m_immediate_command_pool, 1,
                       &command_buffer);
}

uint64_t VulkanContext::submitImmediateCommandsNoWait(
    VkCommandBuffer command_buffer) {
  ASSERT(m_device != VK_NULL_HANDLE);
  ASSERT(command_buffer != VK_NULL_HANDLE);
  ASSERT(m_sync != nullptr);

  const VkResult end_result = vkEndCommandBuffer(command_buffer);
  if (end_result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanContext::submitImmediateCommandsNoWait] vkEndCommandBuffer "
        "failed: {}",
        static_cast<int>(end_result));
  }

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  return m_sync->queueSubmit(m_graphics_queue, submit_info);
}

void VulkanContext::freeImmediateCommandBuffer(VkCommandBuffer command_buffer) {
  ASSERT(m_device != VK_NULL_HANDLE);
  ASSERT(m_immediate_command_pool != VK_NULL_HANDLE);
  if (command_buffer != VK_NULL_HANDLE) {
    vkFreeCommandBuffers(m_device, m_immediate_command_pool, 1,
                         &command_buffer);
  }
}

void VulkanContext::createInstance() {
  uint32_t supported_api_version = VK_API_VERSION_1_1;
  if (vkEnumerateInstanceVersion != nullptr) {
    const VkResult query_result =
        vkEnumerateInstanceVersion(&supported_api_version);
    if (query_result != VK_SUCCESS) {
      LOG_WARN("[VulkanContext::createInstance] vkEnumerateInstanceVersion failed: {}",
               static_cast<int>(query_result));
      supported_api_version = VK_API_VERSION_1_1;
    }
  }

  m_api_version = (supported_api_version >= VK_API_VERSION_1_3)
                      ? VK_API_VERSION_1_3
                      : VK_API_VERSION_1_1;

  // Surface + Win32 surface so the Slint UI's Skia renderer can share this
  // VkInstance and present its swapchain on the editor HWND (Vulkan-unified UI
  // path; the engine itself still renders off-screen). Enabled only when the
  // loader reports them so a pure-headless environment keeps working.
  eastl::vector<const char*> instance_extensions;

  // String literals avoid requiring VK_USE_PLATFORM_WIN32_KHR in this TU.
  constexpr const char* k_surface_extension = "VK_KHR_surface";
  constexpr const char* k_win32_surface_extension = "VK_KHR_win32_surface";
  // Skia's GrVkGpu setup resolves the KHR-suffixed physical-device query entry
  // points (vkGetPhysicalDeviceProperties2KHR, ...) which are only non-null when
  // these instance extensions are enabled. The Slint Vulkan renderer enables
  // them on its self-owned instance, so the shared instance must match or
  // Skia's make_vulkan() fails.
  constexpr const char* k_phys_props2_extension =
      "VK_KHR_get_physical_device_properties2";
  constexpr const char* k_surface_caps2_extension =
      "VK_KHR_get_surface_capabilities2";
  const bool want_surface = m_window_system != nullptr;
  if (want_surface && hasInstanceExtension(k_surface_extension)) {
    instance_extensions.push_back(k_surface_extension);
  }
  if (want_surface && hasInstanceExtension(k_win32_surface_extension)) {
    instance_extensions.push_back(k_win32_surface_extension);
  }
  if (hasInstanceExtension(k_phys_props2_extension)) {
    instance_extensions.push_back(k_phys_props2_extension);
  }
  if (want_surface && hasInstanceExtension(k_surface_caps2_extension)) {
    instance_extensions.push_back(k_surface_caps2_extension);
  }

  if (m_enable_validation &&
      hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  m_enable_validation_layer =
      m_enable_validation && hasLayer(k_validation_layer_name);
  if (m_enable_validation && !m_enable_validation_layer) {
    LOG_WARN("[VulkanContext::createInstance] validation layer unavailable");
  }

  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "BlunderEditor";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.pEngineName = "BlunderEngine";
  app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.apiVersion = m_api_version;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(instance_extensions.size());
  create_info.ppEnabledExtensionNames = instance_extensions.data();

  if (m_enable_validation_layer) {
    // 包含验证层名称
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = &k_validation_layer_name;
  }

  const VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanContext::createInstance] vkCreateInstance failed: {}",
              static_cast<int>(result));
  }
}

void VulkanContext::setupDebugMessenger() {
  if (!m_enable_validation || m_instance == VK_NULL_HANDLE) {
    return;
  }

  auto* create_debug_messenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
  if (!create_debug_messenger) {
    LOG_WARN("[VulkanContext::setupDebugMessenger] extension entry not found");
    return;
  }

  VkDebugUtilsMessengerCreateInfoEXT create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  // 指定希望回调被调用的所有严重性类型
  create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  // 过滤回调会收到的消息类型
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  // 指定回调函数的指针
  create_info.pfnUserCallback = debugUtilsCallback;

  const VkResult result =
      create_debug_messenger(m_instance, &create_info, nullptr, &m_debug_messenger);
  if (result != VK_SUCCESS) {
    LOG_WARN("[VulkanContext::setupDebugMessenger] failed: {}",
             static_cast<int>(result));
  }
}

void VulkanContext::selectPhysicalDevice() {
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
  if (device_count == 0) {
    LOG_FATAL("[VulkanContext::selectPhysicalDevice] no Vulkan GPU found");
  }

  eastl::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

  int best_score = -1;
  VkPhysicalDevice selected_device = VK_NULL_HANDLE;
  uint32_t selected_graphics_family = 0;

  for (VkPhysicalDevice device : devices) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             nullptr);
    if (queue_family_count == 0) {
      continue;
    }

    eastl::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                             queue_families.data());

    bool has_graphics = false;
    uint32_t graphics_family = 0;

    for (uint32_t i = 0; i < queue_family_count; ++i) {
      if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
        has_graphics = true;
        graphics_family = i;
        break;
      }
    }

    if (!has_graphics) {
      continue;
    }

    // 设备属性
    VkPhysicalDeviceProperties device_properties{};
    vkGetPhysicalDeviceProperties(device, &device_properties);

    // 设备特性
    VkPhysicalDeviceVulkan11Features vulkan11_features{};
    vulkan11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

    VkPhysicalDeviceFeatures2 device_features2{};
    device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features2.pNext = &vulkan11_features;
    vkGetPhysicalDeviceFeatures2(device, &device_features2);

    const VkPhysicalDeviceFeatures& device_features = device_features2.features;

    // 内存信息
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

    if (!device_features.geometryShader || !device_features.samplerAnisotropy ||
        !vulkan11_features.shaderDrawParameters ||
        !descriptorIndexingSupportsBindlessTable(device)) {
      continue;
    }

    int score = 0;

    // 设备类型加分
    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += 1000;  // 独显最高优先级
    } else if (device_properties.deviceType ==
               VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
      score += 100;  // 核显其次
    }

    VkDeviceSize vram_size = 0;
    for (uint32_t i = 0; i < memory_properties.memoryHeapCount; i++) {
      // 寻找 DEVICE_LOCAL_BIT 标志，这代表显卡专属的极速显存
      if (memory_properties.memoryHeaps[i].flags &
          VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
        vram_size += memory_properties.memoryHeaps[i].size;
      }
    }

    // 性能指标加分：最大纹理尺寸
    score += device_properties.limits.maxImageDimension2D / 100;

    // 显存大小评分
    score += static_cast<int>(vram_size / (1024 * 1024 * 128));

    if (score > best_score) {
      best_score = score;
      selected_device = device;
      selected_graphics_family = graphics_family;
    }
  }

  if (selected_device == VK_NULL_HANDLE) {
    LOG_FATAL(
        "[VulkanContext::selectPhysicalDevice] no suitable Vulkan device found");
  }

  m_physical_device = selected_device;
  vkGetPhysicalDeviceProperties(m_physical_device, &m_physical_device_properties);
  m_graphics_queue_family = selected_graphics_family;
  uint32_t selected_queue_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &selected_queue_count,
                                           nullptr);
  eastl::vector<VkQueueFamilyProperties> selected_queues(selected_queue_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
      m_physical_device, &selected_queue_count, selected_queues.data());
  if (m_graphics_queue_family < selected_queue_count) {
    m_timestamp_valid_bits =
        selected_queues[m_graphics_queue_family].timestampValidBits;
  }
  const char* type_name = "OTHER";
  switch (m_physical_device_properties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      type_name = "INTEGRATED";
      break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      type_name = "DISCRETE";
      break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      type_name = "VIRTUAL";
      break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      type_name = "CPU";
      break;
    default:
      break;
  }
  LOG_INFO("[VulkanContext] selected device '{}' type={} ({})",
           m_physical_device_properties.deviceName,
           static_cast<int>(m_physical_device_properties.deviceType), type_name);
  if (m_physical_device_properties.deviceType !=
      VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    LOG_WARN(
        "[VulkanContext] selected device is not DISCRETE; GPU timings are "
        "unreliable");
  }
  // Headless mode: there is no present queue. We keep m_present_queue_family
  // mirroring the graphics family so any legacy callers do not crash, but it
  // is not actually used for present operations.
  m_present_queue_family = selected_graphics_family;
}

void VulkanContext::createLogicalDevice() {
  ASSERT(m_physical_device != VK_NULL_HANDLE);

  // The engine renders off-screen; the graphics queue is the only one created.
  // The Slint UI borrows this same queue to present its swapchain (the queue
  // family supports present on desktop GPUs; the UI side verifies it).
  const float queue_priority = 1.0f;

  VkPhysicalDeviceTimelineSemaphoreFeatures supported_timeline{};
  supported_timeline.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;

  VkPhysicalDeviceFragmentShadingRateFeaturesKHR supported_fsr{};
  supported_fsr.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
  supported_fsr.pNext = &supported_timeline;

  VkPhysicalDeviceMeshShaderFeaturesEXT supported_mesh{};
  supported_mesh.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
  supported_mesh.pNext = &supported_fsr;

  VkPhysicalDeviceVulkan12Features supported_vulkan12_features{};
  supported_vulkan12_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

  VkPhysicalDeviceVulkan11Features supported_vulkan11_features{};
  supported_vulkan11_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

  const bool device_vulkan12 =
      VK_VERSION_MINOR(m_physical_device_properties.apiVersion) >= 2;
  if (device_vulkan12) {
    supported_vulkan12_features.pNext = &supported_mesh;
    supported_vulkan11_features.pNext = &supported_vulkan12_features;
  } else {
    supported_vulkan11_features.pNext = &supported_mesh;
  }

  VkPhysicalDeviceFeatures2 supported_features2{};
  supported_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  supported_features2.pNext = &supported_vulkan11_features;
  vkGetPhysicalDeviceFeatures2(m_physical_device, &supported_features2);

  if (!supported_timeline.timelineSemaphore) {
    LOG_FATAL(
        "[VulkanContext::createLogicalDevice] selected Vulkan device does not "
        "support timeline semaphores");
  }

  if (!supported_vulkan11_features.shaderDrawParameters) {
    LOG_FATAL(
        "[VulkanContext::createLogicalDevice] selected Vulkan device does not "
        "support shaderDrawParameters, but the current SPIR-V shaders require it");
  }

  if (!descriptorIndexingSupportsBindlessTable(m_physical_device)) {
    LOG_FATAL(
        "[VulkanContext::createLogicalDevice] selected Vulkan device does not "
        "support descriptor indexing required by the Bindless texture table");
  }

  VkPhysicalDeviceFeatures enabled_features{};
  enabled_features.samplerAnisotropy =
      supported_features2.features.samplerAnisotropy;
  enabled_features.geometryShader = supported_features2.features.geometryShader;
  enabled_features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
  enabled_features.multiDrawIndirect =
      supported_features2.features.multiDrawIndirect;
  enabled_features.drawIndirectFirstInstance =
      supported_features2.features.drawIndirectFirstInstance;
  m_multi_draw_indirect_enabled =
      enabled_features.multiDrawIndirect == VK_TRUE &&
      enabled_features.drawIndirectFirstInstance == VK_TRUE;
  m_sampler_anisotropy_enabled =
      enabled_features.samplerAnisotropy == VK_TRUE;
  m_max_draw_indirect_count =
      m_physical_device_properties.limits.maxDrawIndirectCount;
  if (m_max_draw_indirect_count == 0) {
    m_max_draw_indirect_count = 1;
  }

  VkPhysicalDeviceTimelineSemaphoreFeatures enabled_timeline{};
  enabled_timeline.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
  enabled_timeline.timelineSemaphore = VK_TRUE;

  VkPhysicalDeviceDescriptorIndexingFeatures enabled_indexing{};
  enabled_indexing.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
  enabled_indexing.pNext = &enabled_timeline;
  enabled_indexing.runtimeDescriptorArray = VK_TRUE;
  enabled_indexing.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
  enabled_indexing.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  enabled_indexing.descriptorBindingPartiallyBound = VK_TRUE;
  enabled_indexing.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;

  VkPhysicalDeviceVulkan12Features enabled_vulkan12_features{};
  enabled_vulkan12_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  enabled_vulkan12_features.pNext = &enabled_indexing;
  enabled_vulkan12_features.drawIndirectCount =
      supported_vulkan12_features.drawIndirectCount;
  enabled_vulkan12_features.shaderOutputLayer =
      supported_vulkan12_features.shaderOutputLayer;

  VkPhysicalDeviceVulkan11Features enabled_vulkan11_features{};
  enabled_vulkan11_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  enabled_vulkan11_features.pNext =
      device_vulkan12 ? static_cast<void*>(&enabled_vulkan12_features)
                      : static_cast<void*>(&enabled_indexing);
  enabled_vulkan11_features.shaderDrawParameters = VK_TRUE;

  const bool want_mesh_shaders =
      hasDeviceExtension(m_physical_device, VK_EXT_MESH_SHADER_EXTENSION_NAME) &&
      supported_mesh.taskShader == VK_TRUE &&
      supported_mesh.meshShader == VK_TRUE;

  const bool has_fsr_ext = hasDeviceExtension(
      m_physical_device, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
  const bool has_renderpass2 =
      device_vulkan12 ||
      hasDeviceExtension(m_physical_device,
                         VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
  const bool want_fsr = fragmentShadingRateWanted(
      has_fsr_ext, supported_fsr.attachmentFragmentShadingRate == VK_TRUE,
      supported_fsr.pipelineFragmentShadingRate == VK_TRUE, has_renderpass2);

  VkPhysicalDeviceFragmentShadingRatePropertiesKHR fsr_props{};
  fsr_props.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;
  if (has_fsr_ext) {
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &fsr_props;
    vkGetPhysicalDeviceProperties2(m_physical_device, &props2);
  }

  VkPhysicalDeviceMeshShaderFeaturesEXT enabled_mesh{};
  enabled_mesh.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
  enabled_mesh.taskShader = VK_TRUE;
  enabled_mesh.meshShader = VK_TRUE;
  if (want_mesh_shaders) {
    enabled_mesh.pNext = &enabled_vulkan11_features;
  }

  VkPhysicalDeviceFragmentShadingRateFeaturesKHR enabled_fsr{};
  enabled_fsr.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
  enabled_fsr.attachmentFragmentShadingRate = VK_TRUE;
  enabled_fsr.pipelineFragmentShadingRate = VK_TRUE;
  if (want_fsr) {
    enabled_fsr.pNext = want_mesh_shaders
                            ? static_cast<void*>(&enabled_mesh)
                            : static_cast<void*>(&enabled_vulkan11_features);
  }

  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = m_graphics_queue_family;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext =
      want_fsr ? static_cast<void*>(&enabled_fsr)
               : (want_mesh_shaders ? static_cast<void*>(&enabled_mesh)
                                    : static_cast<void*>(&enabled_vulkan11_features));
  create_info.queueCreateInfoCount = 1;
  create_info.pQueueCreateInfos = &queue_create_info;
  create_info.pEnabledFeatures = &enabled_features;
  // VK_KHR_swapchain so the shared logical device can drive the Slint UI
  // swapchain (the engine renders off-screen and never presents itself).
  eastl::vector<const char*> device_extensions;
  device_extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
  if (VK_VERSION_MINOR(m_physical_device_properties.apiVersion) < 2) {
    if (!hasDeviceExtension(m_physical_device,
                           VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) {
      LOG_FATAL(
          "[VulkanContext::createLogicalDevice] selected Vulkan device does "
          "not support VK_KHR_timeline_semaphore");
    }
    device_extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
  }
  constexpr const char* k_swapchain_extension = "VK_KHR_swapchain";
  if (m_window_system != nullptr &&
      hasDeviceExtension(m_physical_device, k_swapchain_extension)) {
    device_extensions.push_back(k_swapchain_extension);
  }
  if (want_mesh_shaders) {
    device_extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
  }
  if (want_fsr) {
    device_extensions.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
    if (!device_vulkan12 &&
        hasDeviceExtension(m_physical_device,
                           VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)) {
      device_extensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    }
  }
  const bool want_khr_draw_indirect_count =
      !device_vulkan12 || supported_vulkan12_features.drawIndirectCount != VK_TRUE;
  if (want_khr_draw_indirect_count &&
      hasDeviceExtension(m_physical_device,
                         VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME)) {
    device_extensions.push_back(VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME);
  }
  m_calibrated_timestamps_enabled = hasDeviceExtension(
      m_physical_device, VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
  if (m_calibrated_timestamps_enabled) {
    device_extensions.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
  }
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(device_extensions.size());
  create_info.ppEnabledExtensionNames =
      device_extensions.empty() ? nullptr : device_extensions.data();
  // Validation layers are instance-only (Vulkan 1.0+); device layer fields must stay zero.
  create_info.enabledLayerCount = 0;
  create_info.ppEnabledLayerNames = nullptr;

  const VkResult result =
      vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanContext::createLogicalDevice] failed: {}",
              static_cast<int>(result));
  }

  vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
  m_present_queue = m_graphics_queue;

  m_mesh_shaders_enabled = want_mesh_shaders;
  m_shader_output_layer_enabled =
      device_vulkan12 && enabled_vulkan12_features.shaderOutputLayer == VK_TRUE;
  LOG_INFO("[VulkanContext] shaderOutputLayer={}",
           m_shader_output_layer_enabled ? 1 : 0);
  if (m_mesh_shaders_enabled) {
    m_cmd_draw_mesh_tasks_ext = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
        vkGetDeviceProcAddr(m_device, "vkCmdDrawMeshTasksEXT"));
    if (m_cmd_draw_mesh_tasks_ext == nullptr) {
      LOG_WARN(
          "[VulkanContext] VK_EXT_mesh_shader enabled but "
          "vkCmdDrawMeshTasksEXT is null; falling back to compute+VS/FS");
      m_mesh_shaders_enabled = false;
    } else {
      LOG_INFO("[VulkanContext] VK_EXT_mesh_shader enabled");
    }
  } else {
    LOG_INFO(
        "[VulkanContext] VK_EXT_mesh_shader absent; GPU-driven uses "
        "compute+VS/FS");
  }

  m_fragment_shading_rate_enabled = want_fsr;
  if (m_fragment_shading_rate_enabled) {
    m_cmd_set_fragment_shading_rate_khr =
        reinterpret_cast<PFN_vkCmdSetFragmentShadingRateKHR>(
            vkGetDeviceProcAddr(m_device, "vkCmdSetFragmentShadingRateKHR"));
    m_create_render_pass2 = reinterpret_cast<PFN_vkCreateRenderPass2>(
        vkGetDeviceProcAddr(m_device, "vkCreateRenderPass2"));
    if (m_create_render_pass2 == nullptr) {
      m_create_render_pass2 = reinterpret_cast<PFN_vkCreateRenderPass2>(
          vkGetDeviceProcAddr(m_device, "vkCreateRenderPass2KHR"));
    }
    if (m_cmd_set_fragment_shading_rate_khr == nullptr ||
        m_create_render_pass2 == nullptr) {
      LOG_WARN(
          "[VulkanContext] VK_KHR_fragment_shading_rate enabled but "
          "vkCmdSetFragmentShadingRateKHR or vkCreateRenderPass2 is null; "
          "lighting stays 1×1");
      m_fragment_shading_rate_enabled = false;
      m_cmd_set_fragment_shading_rate_khr = nullptr;
      m_create_render_pass2 = nullptr;
    } else {
      uint32_t texel_w = 1;
      uint32_t texel_h = 1;
      chooseFragmentShadingRateTexelSize(
          fsr_props.minFragmentShadingRateAttachmentTexelSize.width,
          fsr_props.minFragmentShadingRateAttachmentTexelSize.height,
          fsr_props.maxFragmentShadingRateAttachmentTexelSize.width,
          fsr_props.maxFragmentShadingRateAttachmentTexelSize.height, &texel_w,
          &texel_h);
      m_fragment_shading_rate_min_texel =
          fsr_props.minFragmentShadingRateAttachmentTexelSize;
      m_fragment_shading_rate_max_texel =
          fsr_props.maxFragmentShadingRateAttachmentTexelSize;
      m_fragment_shading_rate_texel = {texel_w, texel_h};
      LOG_INFO(
          "[VulkanContext] VK_KHR_fragment_shading_rate enabled (texel {}x{})",
          texel_w, texel_h);
    }
  } else {
    LOG_INFO(
        "[VulkanContext] VK_KHR_fragment_shading_rate absent; lighting stays "
        "1×1");
  }

  m_cmd_draw_indexed_indirect_count =
      reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCount>(
          vkGetDeviceProcAddr(m_device, "vkCmdDrawIndexedIndirectCount"));
  if (m_cmd_draw_indexed_indirect_count == nullptr) {
    m_cmd_draw_indexed_indirect_count =
        reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCount>(
            vkGetDeviceProcAddr(m_device, "vkCmdDrawIndexedIndirectCountKHR"));
  }
  m_draw_indirect_count_enabled = m_cmd_draw_indexed_indirect_count != nullptr &&
                                  m_multi_draw_indirect_enabled;
  if (m_draw_indirect_count_enabled) {
    LOG_INFO("[VulkanContext] drawIndirectCount enabled (max {})",
             m_max_draw_indirect_count);
  } else {
    LOG_INFO(
        "[VulkanContext] drawIndirectCount absent; GPU-driven uses sparse "
        "vkCmdDrawIndexedIndirect");
  }

  createImmediateCommandPool();
  m_secondary_command_buffers.initialize(this);
}

void VulkanContext::createImmediateCommandPool() {
  ASSERT(m_device != VK_NULL_HANDLE);

  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = m_graphics_queue_family;

  const VkResult result = vkCreateCommandPool(
      m_device, &pool_info, nullptr, &m_immediate_command_pool);
  if (result != VK_SUCCESS) {
    LOG_FATAL(
        "[VulkanContext::createImmediateCommandPool] vkCreateCommandPool "
        "failed: {}",
        static_cast<int>(result));
  }
}

void VulkanContext::createPipelineCache() {
  ASSERT(m_device != VK_NULL_HANDLE);

  eastl::vector<uint8_t> blob = tryLoadPipelineCacheBlob(
      m_physical_device_properties.pipelineCacheUUID,
      m_slang_build_tag.c_str());

  VkPipelineCacheCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  if (!blob.empty()) {
    info.initialDataSize = blob.size();
    info.pInitialData = blob.data();
  }

  VkResult result =
      vkCreatePipelineCache(m_device, &info, nullptr, &m_pipeline_cache);
  if (result != VK_SUCCESS) {
    LOG_WARN(
        "[VulkanContext] vkCreatePipelineCache failed ({}); rebuilding empty "
        "cache",
        static_cast<int>(result));
    deletePipelineCacheBlob(m_physical_device_properties.pipelineCacheUUID,
                            m_slang_build_tag.c_str());
    m_pipeline_cache = VK_NULL_HANDLE;
    info.initialDataSize = 0;
    info.pInitialData = nullptr;
    result =
        vkCreatePipelineCache(m_device, &info, nullptr, &m_pipeline_cache);
    if (result != VK_SUCCESS) {
      LOG_WARN("[VulkanContext] empty VkPipelineCache create failed ({})",
               static_cast<int>(result));
      m_pipeline_cache = VK_NULL_HANDLE;
    }
  } else if (!blob.empty()) {
    LOG_INFO("[VulkanContext] loaded Pipeline cache ({} bytes)", blob.size());
  }
}

void VulkanContext::savePipelineCache() {
  if (m_device == VK_NULL_HANDLE || m_pipeline_cache == VK_NULL_HANDLE) {
    return;
  }
  size_t size = 0;
  VkResult result =
      vkGetPipelineCacheData(m_device, m_pipeline_cache, &size, nullptr);
  if (result != VK_SUCCESS) {
    LOG_WARN("[VulkanContext] vkGetPipelineCacheData size query failed ({})",
             static_cast<int>(result));
    return;
  }
  if (size == 0) {
    return;
  }
  eastl::vector<uint8_t> blob(size);
  result =
      vkGetPipelineCacheData(m_device, m_pipeline_cache, &size, blob.data());
  if (result != VK_SUCCESS) {
    LOG_WARN("[VulkanContext] vkGetPipelineCacheData failed ({})",
             static_cast<int>(result));
    return;
  }
  blob.resize(size);
  tryStorePipelineCacheBlob(m_physical_device_properties.pipelineCacheUUID,
                            m_slang_build_tag.c_str(), blob.data(),
                            blob.size());
}

void VulkanContext::destroyPipelineCache() {
  if (m_device != VK_NULL_HANDLE && m_pipeline_cache != VK_NULL_HANDLE) {
    vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
    m_pipeline_cache = VK_NULL_HANDLE;
  }
}

void VulkanContext::recreateEmptyPipelineCache() {
  destroyPipelineCache();
  if (m_device == VK_NULL_HANDLE) {
    return;
  }
  VkPipelineCacheCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  const VkResult result =
      vkCreatePipelineCache(m_device, &info, nullptr, &m_pipeline_cache);
  if (result != VK_SUCCESS) {
    LOG_WARN("[VulkanContext] empty VkPipelineCache recreate failed ({})",
             static_cast<int>(result));
    m_pipeline_cache = VK_NULL_HANDLE;
  }
}

VkResult VulkanContext::createGraphicsPipelines(
    uint32_t create_info_count,
    const VkGraphicsPipelineCreateInfo* create_infos, VkPipeline* pipelines) {
  ASSERT(m_device != VK_NULL_HANDLE);
  ASSERT(create_infos);
  ASSERT(pipelines);

  VkResult result = vkCreateGraphicsPipelines(
      m_device, m_pipeline_cache, create_info_count, create_infos, nullptr,
      pipelines);
  if (result != VK_SUCCESS && m_pipeline_cache != VK_NULL_HANDLE) {
    LOG_WARN(
        "[VulkanContext] vkCreateGraphicsPipelines failed ({}); retrying with "
        "empty Pipeline cache",
        static_cast<int>(result));
    for (uint32_t i = 0; i < create_info_count; ++i) {
      if (pipelines[i] != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, pipelines[i], nullptr);
        pipelines[i] = VK_NULL_HANDLE;
      }
    }
    recreateEmptyPipelineCache();
    result = vkCreateGraphicsPipelines(m_device, m_pipeline_cache,
                                       create_info_count, create_infos, nullptr,
                                       pipelines);
    if (result == VK_SUCCESS) {
      deletePipelineCacheBlob(m_physical_device_properties.pipelineCacheUUID,
                              m_slang_build_tag.c_str());
    }
  }
  return result;
}

}  // namespace Blunder
