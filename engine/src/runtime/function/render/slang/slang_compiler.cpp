#include "runtime/function/render/slang/slang_compiler.h"

#include <slang-com-ptr.h>
#include <slang.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "runtime/core/base/macro.h"

namespace Blunder {

namespace {

/// Read the entire contents of a text file into a string.
eastl::string readFileToString(const char* path) {
  namespace fs = std::filesystem;

  // Search relative to cwd and parent directories (matching engine convention)
  const fs::path file_path(path);
  eastl::vector<fs::path> candidates;
  candidates.reserve(8);
  candidates.emplace_back(file_path);

  std::error_code ec;
  fs::path current = fs::current_path(ec);
  if (!ec) {
    while (!current.empty()) {
      candidates.emplace_back(current / file_path);
      const fs::path parent = current.parent_path();
      if (parent == current) break;
      current = parent;
    }
  }

  for (const fs::path& candidate : candidates) {
    std::ifstream file(candidate, std::ios::ate | std::ios::binary);
    if (file.is_open()) {
      const auto file_size = file.tellg();
      if (file_size <= 0) continue;
      eastl::string content;
      content.resize(static_cast<size_t>(file_size));
      file.seekg(0);
      file.read(content.data(), file_size);
      if (file) {
        LOG_DEBUG("[SlangCompiler] loaded shader source: {}",
                  candidate.string());
        return content;
      }
    }
  }

  LOG_FATAL("[SlangCompiler] failed to open shader file: {}", path);
  return {};  // unreachable (LOG_FATAL throws)
}

/// Convert Slang diagnostic blob to a loggable string.
eastl::string diagnosticsToString(slang::IBlob* diagnostics) {
  if (!diagnostics || diagnostics->getBufferSize() == 0) {
    return {};
  }
  return eastl::string(
      static_cast<const char*>(diagnostics->getBufferPointer()),
      diagnostics->getBufferSize());
}

}  // namespace

SlangCompiler::~SlangCompiler() { shutdown(); }

void SlangCompiler::initialize() {
  if (m_global_session) return;

  SlangResult result = slang::createGlobalSession(
      reinterpret_cast<slang::IGlobalSession**>(&m_global_session));
  if (SLANG_FAILED(result) || !m_global_session) {
    LOG_FATAL(
        "[SlangCompiler::initialize] failed to create Slang global session");
  }

  LOG_INFO("[SlangCompiler] initialized Slang shader compiler");
}

void SlangCompiler::shutdown() {
  if (m_global_session) {
    m_global_session->release();
    m_global_session = nullptr;
    LOG_INFO("[SlangCompiler] shutdown");
  }
}

SlangCompiler::ShaderResult SlangCompiler::compileShader(
    const char* source_path, const char* entry_point, int stage) {
  ASSERT(m_global_session);
  ASSERT(source_path);
  ASSERT(entry_point);

  // 从磁盘读取着色器源代码
  eastl::string source_code = readFileToString(source_path);

  // 配置 Slang 会话以生成 SPIR-V 代码
  slang::TargetDesc target_desc{};
  target_desc.format = SLANG_SPIRV;
  target_desc.profile = m_global_session->findProfile("spirv_1_5");

  slang::CompilerOptionEntry options[] = {
      // 生成 SPIR-V 直接输出（而不是通过 GLSL 中间表示）
      {slang::CompilerOptionName::EmitSpirvDirectly,
       {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
      // 入口点名称保留在 SPIR-V 中（而不是重命名为 "main"）
      {slang::CompilerOptionName::VulkanUseEntryPointName,
       {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
  };

  slang::SessionDesc session_desc{};
  session_desc.targets = &target_desc;
  session_desc.targetCount = 1;
  session_desc.compilerOptionEntries = options;
  session_desc.compilerOptionEntryCount = sizeof(options) / sizeof(options[0]);
  session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

  Slang::ComPtr<slang::ISession> session;
  SlangResult result =
      m_global_session->createSession(session_desc, session.writeRef());
  if (SLANG_FAILED(result) || !session) {
    LOG_FATAL("[SlangCompiler::compileShader] failed to create Slang session");
  }

  // 从源代码字符串加载模块（而不是直接从文件加载），以便我们可以提供更准确的虚拟路径用于诊断信息。
  Slang::ComPtr<slang::IBlob> diagnostics_blob;
  Slang::ComPtr<slang::IModule> shader_module(
      session->loadModuleFromSourceString("shader",     // 模块名称
                                          source_path,  // 虚拟路径（用于诊断）
                                          source_code.c_str(),
                                          diagnostics_blob.writeRef()));

  if (diagnostics_blob && diagnostics_blob->getBufferSize() > 0) {
    eastl::string diag = diagnosticsToString(diagnostics_blob.get());
    LOG_WARN("[SlangCompiler] compilation diagnostics:\n{}", diag);
  }

  if (!shader_module) {
    LOG_FATAL("[SlangCompiler::compileShader] failed to load module from: {}",
              source_path);
  }

  // 查找指定名称的入口点对象
  Slang::ComPtr<slang::IEntryPoint> entry_point_obj;
  result = shader_module->findEntryPointByName(entry_point,
                                               entry_point_obj.writeRef());
  if (SLANG_FAILED(result) || !entry_point_obj) {
    LOG_FATAL(
        "[SlangCompiler::compileShader] entry point '{}' not found in: {}",
        entry_point, source_path);
  }

  // 组合模块和入口点以创建一个完整的程序组件，准备进行链接和代码生成
  slang::IComponentType* components[] = {shader_module, entry_point_obj};
  Slang::ComPtr<slang::IComponentType> composed_program;
  diagnostics_blob = nullptr;
  result = session->createCompositeComponentType(
      components, 2, composed_program.writeRef(), diagnostics_blob.writeRef());

  if (diagnostics_blob && diagnostics_blob->getBufferSize() > 0) {
    eastl::string diag = diagnosticsToString(diagnostics_blob.get());
    LOG_WARN("[SlangCompiler] composition diagnostics:\n{}", diag);
  }

  if (SLANG_FAILED(result) || !composed_program) {
    LOG_FATAL(
        "[SlangCompiler::compileShader] failed to compose program for: {}",
        source_path);
  }

  // 链接
  Slang::ComPtr<slang::IComponentType> linked_program;
  diagnostics_blob = nullptr;
  result = composed_program->link(linked_program.writeRef(),
                                  diagnostics_blob.writeRef());

  if (diagnostics_blob && diagnostics_blob->getBufferSize() > 0) {
    eastl::string diag = diagnosticsToString(diagnostics_blob.get());
    LOG_WARN("[SlangCompiler] link diagnostics:\n{}", diag);
  }

  if (SLANG_FAILED(result) || !linked_program) {
    LOG_FATAL("[SlangCompiler::compileShader] failed to link program for: {}",
              source_path);
  }

  // 获取 SPIR-V 代码
  Slang::ComPtr<slang::IBlob> spirv_blob;
  diagnostics_blob = nullptr;
  result = linked_program->getEntryPointCode(0,  // 入口点索引
                                             0,  // 目标索引
                                             spirv_blob.writeRef(),
                                             diagnostics_blob.writeRef());

  if (diagnostics_blob && diagnostics_blob->getBufferSize() > 0) {
    eastl::string diag = diagnosticsToString(diagnostics_blob.get());
    LOG_WARN("[SlangCompiler] code generation diagnostics:\n{}", diag);
  }

  if (SLANG_FAILED(result) || !spirv_blob) {
    LOG_FATAL(
        "[SlangCompiler::compileShader] failed to generate SPIR-V for: {}",
        source_path);
  }

  // 复制 SPIR-V 数据到结果结构中
  ShaderResult shader_result;
  shader_result.entry_point_name =
      eastl::string(entry_point, std::strlen(entry_point));
  const size_t spirv_size = spirv_blob->getBufferSize();
  shader_result.spirv_code.resize(spirv_size);
  std::memcpy(shader_result.spirv_code.data(), spirv_blob->getBufferPointer(),
              spirv_size);

  LOG_INFO("[SlangCompiler] compiled '{}' entry '{}' -> {} bytes SPIR-V",
           source_path, entry_point, spirv_size);

  return shader_result;
}

}  // namespace Blunder
