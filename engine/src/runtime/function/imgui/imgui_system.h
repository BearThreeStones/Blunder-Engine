#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Blunder {

class VulkanContext;

struct ImGuiSystemInitInfo {
  GLFWwindow* window{nullptr};
  VulkanContext* vulkan_context{nullptr};
  VkRenderPass render_pass{VK_NULL_HANDLE};
  uint32_t image_count{0};
};

class ImGuiSystem final {
 public:
  ImGuiSystem() = default;
  ~ImGuiSystem() = default;

  void initialize(const ImGuiSystemInitInfo& info);
  void shutdown();

  void beginFrame();
  void endFrame();

  /// Record ImGui draw commands into the given command buffer.
  /// Must be called between vkCmdBeginRenderPass and vkCmdEndRenderPass.
  void render(VkCommandBuffer command_buffer);

  void setShowDemoWindow(bool show_demo_window);

 private:
  static void checkVkResult(int err);
  void createDescriptorPool();

  GLFWwindow* m_window{nullptr};
  VulkanContext* m_context{nullptr};
  VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};

  bool m_initialized{false};
  bool m_show_demo_window{true};
};

}  // namespace Blunder
