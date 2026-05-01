#include "runtime/function/render/vulkan/vulkan_context.h"

#include <cstring>

#include <SDL3/SDL.h>

#include "EASTL/set.h"
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/platform/window/window_system.h"

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

}  // namespace

void VulkanContext::initialize(const VulkanContextCreateInfo& info) {
  ASSERT(info.window_system);

  m_window_system = info.window_system;
#ifdef NDEBUG // 非调试模式
  m_enable_validation = false;
#else
  m_enable_validation = info.enable_validation;
#endif

  LOG_INFO("[VulkanContext::initialize] creating Vulkan instance");
  createInstance();
  LOG_INFO("[VulkanContext::initialize] setting up debug messenger");
  setupDebugMessenger();
  LOG_INFO("[VulkanContext::initialize] creating window surface");
  createSurface();
  LOG_INFO("[VulkanContext::initialize] selecting physical device");
  selectPhysicalDevice();
  LOG_INFO("[VulkanContext::initialize] creating logical device");
  createLogicalDevice();

  LOG_INFO("[VulkanContext::initialize] Vulkan context initialized");
}

void VulkanContext::shutdown() {
  LOG_INFO("[VulkanContext::shutdown] tearing down Vulkan context");

  if (m_device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_device);
  }

  if (m_device != VK_NULL_HANDLE) {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }

  if (m_surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
  }

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
  m_window_system = nullptr;
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

  uint32_t window_extension_count = 0;
  eastl::array<const char*, 16> window_extensions =
      m_window_system->getRequiredVulkanExtensions(&window_extension_count);

  eastl::vector<const char*> instance_extensions(window_extensions.begin(),
                                                 window_extensions.begin() +
                                                     window_extension_count);

  if (m_enable_validation &&
      hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  const bool enable_validation_layer =
      m_enable_validation && hasLayer(k_validation_layer_name);
  if (m_enable_validation && !enable_validation_layer) {
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

  if (enable_validation_layer) {
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

void VulkanContext::createSurface() {
  ASSERT(m_window_system);
  ASSERT(m_instance != VK_NULL_HANDLE);

  if (!m_window_system->createVulkanSurface(m_instance, &m_surface)) {
    LOG_FATAL("[VulkanContext::createSurface] failed: {}",
              SDL_GetError());
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
  uint32_t selected_present_family = 0;

  for (VkPhysicalDevice device : devices) {
    if (!hasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
      continue;
    }

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
    bool has_present = false;
    uint32_t graphics_family = 0;
    uint32_t present_family = 0;

    // 寻找同时支持图形和展示的队列族
    for (uint32_t i = 0; i < queue_family_count; ++i) {
      if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
        has_graphics = true;
        graphics_family = i;
      }

      VkBool32 present_support = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);
      if (present_support == VK_TRUE) {
        has_present = true;
        present_family = i;
      }
    }

    if (!has_graphics || !has_present) {
      continue;
    }

    // 设备属性
    VkPhysicalDeviceProperties device_properties{};
    vkGetPhysicalDeviceProperties(device, &device_properties);

    // 设备特性
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);

    // 内存信息
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device, &memory_properties);

    if (!device_features.geometryShader || !device_features.samplerAnisotropy) {
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
      selected_present_family = present_family;
    }
  }

  if (selected_device == VK_NULL_HANDLE) {
    LOG_FATAL(
        "[VulkanContext::selectPhysicalDevice] no suitable Vulkan device found");
  }

  m_physical_device = selected_device;
  m_graphics_queue_family = selected_graphics_family;
  m_present_queue_family = selected_present_family;
}

void VulkanContext::createLogicalDevice() {
  ASSERT(m_physical_device != VK_NULL_HANDLE);

  eastl::set<uint32_t> queue_families = {m_graphics_queue_family,
                                       m_present_queue_family};

  const float queue_priority = 1.0f;

  // 为队列族创建队列
  eastl::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  queue_create_infos.reserve(queue_families.size());

  for (uint32_t queue_family : queue_families) {
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);
  }

  const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  // 创建逻辑设备
  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
  create_info.pQueueCreateInfos = queue_create_infos.data();
  create_info.enabledExtensionCount = 1;
  create_info.ppEnabledExtensionNames = device_extensions;
  // 为了兼容较旧的实现，仍设置 enabledLayerCount 和 ppEnabledLayerNames 字段
  create_info.enabledLayerCount = 1;
  create_info.ppEnabledLayerNames = &k_validation_layer_name;

  const VkResult result =
      vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanContext::createLogicalDevice] failed: {}",
              static_cast<int>(result));
  }

  // 检索队列句柄
  vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
  vkGetDeviceQueue(m_device, m_present_queue_family, 0, &m_present_queue);
}

}  // namespace Blunder
