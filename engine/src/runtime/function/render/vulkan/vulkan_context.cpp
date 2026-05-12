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

  // Headless mode: Slint Skia owns presentation on the window's HWND. The
  // engine's Vulkan context only renders to off-screen images and reads
  // them back to CPU; we no longer need a surface, swapchain, or present
  // queue here.
  LOG_INFO("[VulkanContext::initialize] creating Vulkan instance");
  createInstance();
  LOG_INFO("[VulkanContext::initialize] setting up debug messenger");
  setupDebugMessenger();
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

  // Headless: no window surface extensions required.
  eastl::vector<const char*> instance_extensions;

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
        !vulkan11_features.shaderDrawParameters) {
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
  m_graphics_queue_family = selected_graphics_family;
  // Headless mode: there is no present queue. We keep m_present_queue_family
  // mirroring the graphics family so any legacy callers do not crash, but it
  // is not actually used for present operations.
  m_present_queue_family = selected_graphics_family;
}

void VulkanContext::createLogicalDevice() {
  ASSERT(m_physical_device != VK_NULL_HANDLE);

  // Headless: only the graphics queue is required; no swapchain extension.
  const float queue_priority = 1.0f;

  VkPhysicalDeviceVulkan11Features supported_vulkan11_features{};
  supported_vulkan11_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

  VkPhysicalDeviceFeatures2 supported_features2{};
  supported_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  supported_features2.pNext = &supported_vulkan11_features;
  vkGetPhysicalDeviceFeatures2(m_physical_device, &supported_features2);

  if (!supported_vulkan11_features.shaderDrawParameters) {
    LOG_FATAL(
        "[VulkanContext::createLogicalDevice] selected Vulkan device does not "
        "support shaderDrawParameters, but the current SPIR-V shaders require it");
  }

  VkPhysicalDeviceFeatures enabled_features{};
  enabled_features.samplerAnisotropy =
      supported_features2.features.samplerAnisotropy;
  enabled_features.geometryShader = supported_features2.features.geometryShader;

  VkPhysicalDeviceVulkan11Features enabled_vulkan11_features{};
  enabled_vulkan11_features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  enabled_vulkan11_features.shaderDrawParameters = VK_TRUE;

  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = m_graphics_queue_family;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext = &enabled_vulkan11_features;
  create_info.queueCreateInfoCount = 1;
  create_info.pQueueCreateInfos = &queue_create_info;
  create_info.pEnabledFeatures = &enabled_features;
  create_info.enabledExtensionCount = 0;
  create_info.ppEnabledExtensionNames = nullptr;
  // 为了兼容较旧的实现，仅在验证层可用时设置 layer 字段。
  if (m_enable_validation_layer) {
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = &k_validation_layer_name;
  }

  const VkResult result =
      vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanContext::createLogicalDevice] failed: {}",
              static_cast<int>(result));
  }

  vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
  m_present_queue = m_graphics_queue;
}

}  // namespace Blunder
