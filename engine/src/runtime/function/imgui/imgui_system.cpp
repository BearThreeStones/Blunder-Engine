#include "imgui_system.h"

#ifdef IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#undef IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#endif

#ifdef VK_NO_PROTOTYPES
#undef VK_NO_PROTOTYPES
#endif

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

namespace {

PFN_vkVoidFunction loadVulkanFunction(const char* function_name,
                                      void* user_data) {
  VkInstance instance = reinterpret_cast<VkInstance>(user_data);
  return vkGetInstanceProcAddr(instance, function_name);
}

}  // namespace

void ImGuiSystem::checkVkResult(int err) {
  if (err == VK_SUCCESS) {
    return;
  }

  LOG_ERROR("[ImGuiSystem::Vulkan] VkResult={}", err);
  if (err < 0) {
    LOG_FATAL("[ImGuiSystem::Vulkan] fatal VkResult={}", err);
  }
}

void ImGuiSystem::createDescriptorPool() {
  ASSERT(m_context);

  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
       IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE * 16},
  };

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 0;
  for (VkDescriptorPoolSize& pool_size : pool_sizes) {
    pool_info.maxSets += pool_size.descriptorCount;
  }
  pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
  pool_info.pPoolSizes = pool_sizes;

  checkVkResult(vkCreateDescriptorPool(m_context->getDevice(), &pool_info,
                                       nullptr, &m_descriptor_pool));
}

void ImGuiSystem::initialize(const ImGuiSystemInitInfo& info) {
  ASSERT(info.window);
  ASSERT(info.vulkan_context);
  ASSERT(info.render_pass != VK_NULL_HANDLE);
  ASSERT(info.image_count > 0);

  LOG_INFO("[ImGuiSystem::initialize] initializing ImGui with engine render pass");

  m_window = info.window;
  m_context = info.vulkan_context;

  const bool vulkan_functions_loaded = ImGui_ImplVulkan_LoadFunctions(
      info.vulkan_context->getApiVersion(), loadVulkanFunction,
      info.vulkan_context->getInstance());
  ASSERT(vulkan_functions_loaded);

  createDescriptorPool();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGui::StyleColorsDark();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  ImGui_ImplGlfw_InitForVulkan(m_window, true);

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.ApiVersion = info.vulkan_context->getApiVersion();
  init_info.Instance = info.vulkan_context->getInstance();
  init_info.PhysicalDevice = info.vulkan_context->getPhysicalDevice();
  init_info.Device = info.vulkan_context->getDevice();
  init_info.QueueFamily = info.vulkan_context->getGraphicsQueueFamily();
  init_info.Queue = info.vulkan_context->getGraphicsQueue();
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPool = m_descriptor_pool;
  init_info.MinImageCount = 2;
  init_info.ImageCount = info.image_count;
  init_info.Allocator = nullptr;
  init_info.PipelineInfoMain.RenderPass = info.render_pass;
  init_info.PipelineInfoMain.Subpass = 0;
  init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.CheckVkResultFn =
      [](VkResult err) { ImGuiSystem::checkVkResult(static_cast<int>(err)); };

  ImGui_ImplVulkan_Init(&init_info);

  m_initialized = true;
}

void ImGuiSystem::shutdown() {
  if (!m_context) {
    return;
  }

  LOG_INFO("[ImGuiSystem::shutdown] shutting down ImGui");

  vkDeviceWaitIdle(m_context->getDevice());

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (m_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_context->getDevice(), m_descriptor_pool, nullptr);
    m_descriptor_pool = VK_NULL_HANDLE;
  }

  m_context = nullptr;
  m_window = nullptr;
  m_initialized = false;
}

void ImGuiSystem::beginFrame() {
  if (!m_initialized) {
    return;
  }

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  if (m_show_demo_window) {
    ImGui::ShowDemoWindow(&m_show_demo_window);
  }
}

void ImGuiSystem::endFrame() {
  if (!m_initialized) {
    return;
  }

  ImGui::Render();

  ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

void ImGuiSystem::render(VkCommandBuffer command_buffer) {
  if (!m_initialized) {
    return;
  }

  ImDrawData* draw_data = ImGui::GetDrawData();
  if (draw_data) {
    ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
  }
}

void ImGuiSystem::setShowDemoWindow(bool show_demo_window) {
  m_show_demo_window = show_demo_window;
}

}  // namespace Blunder
