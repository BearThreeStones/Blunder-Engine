#pragma once

#include <vulkan/vulkan.h>

#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

class SlangCompiler;

/// 通过 SlangCompiler 编译 .slang 着色器文件并创建 VkShaderModule。
class VulkanShader final {
 public:
  /// 描述一个已编译的着色器阶段，已准备好用于创建管线。
  struct ShaderStage {
    VkShaderModule module{VK_NULL_HANDLE};
    VkShaderStageFlagBits stage_flags{};
    eastl::string entry_point;  // 例如 "vertexMain", "fragmentMain"
  };

  /// loadFromSlang() 的入口点规范。
  struct EntryPointSpec {
    const char* name;                   // 例如 "vertexMain"
    VkShaderStageFlagBits stage_flags;  // 例如 VK_SHADER_STAGE_VERTEX_BIT
    int slang_stage;                    // 例如 SLANG_STAGE_VERTEX
  };

  /// 编译 .slang 文件并为每个入口点创建 VkShaderModule。
  ///
  /// @param device      Vulkan 逻辑设备。
  /// @param compiler    已初始化的 SlangCompiler 实例。
  /// @param slang_path  .slang 源文件路径。
  /// @param entries     要编译的入口点列表。
  /// @return 返回 ShaderStage 向量，每个入口点一个。
  static eastl::vector<ShaderStage> loadFromSlang(
      VkDevice device, SlangCompiler* compiler, const char* slang_path,
      const eastl::vector<EntryPointSpec>& entries);

  /// 销毁 VkShaderModule（将句柄置为 VK_NULL_HANDLE）。
  static void destroyShaderModule(VkDevice device,
                                  VkShaderModule* shader_module);
};

}  // namespace Blunder
