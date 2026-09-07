#include "runtime/function/render/slang/slang_compiler.h"

#include <slang-com-ptr.h>
#include <slang.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/engine_gpu_cache.h"
#include "runtime/function/render/slang/sha256.h"

namespace Blunder {

namespace {

eastl::string readFileToString(const char* path) {
  namespace fs = std::filesystem;

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
        return content;
      }
    }
  }

  LOG_FATAL("[SlangCompiler] failed to open shader file: {}", path);
  return {};
}

eastl::string diagnosticsToString(slang::IBlob* diagnostics) {
  if (!diagnostics || diagnostics->getBufferSize() == 0) {
    return {};
  }
  return eastl::string(
      static_cast<const char*>(diagnostics->getBufferPointer()),
      diagnostics->getBufferSize());
}

void logDiagnostics(const char* label, slang::IBlob* diagnostics) {
  if (diagnostics && diagnostics->getBufferSize() > 0) {
    LOG_WARN("[SlangCompiler] {}:\n{}", label, diagnosticsToString(diagnostics));
  }
}

const char* slangBuildTag(slang::IGlobalSession* session) {
  const char* tag = session->getBuildTagString();
  if (tag == nullptr || tag[0] == '\0') {
    return "unknown";
  }
  return tag;
}

void hashSourceBytes(const eastl::string& source, uint8_t out[32]) {
  sha256(source.data(), source.size(), out);
}

bool isDescriptorParameter(slang::VariableLayoutReflection* var) {
  if (var == nullptr || var->getTypeLayout() == nullptr) {
    return false;
  }
  const slang::ParameterCategory category = var->getCategory();
  if (category == slang::ParameterCategory::VaryingInput ||
      category == slang::ParameterCategory::VaryingOutput ||
      category == slang::ParameterCategory::Uniform ||
      category == slang::ParameterCategory::None ||
      category == slang::ParameterCategory::PushConstantBuffer) {
    return false;
  }
  if (category == slang::ParameterCategory::Mixed) {
    const unsigned count = var->getCategoryCount();
    for (unsigned i = 0; i < count; ++i) {
      const slang::ParameterCategory inner = var->getCategoryByIndex(i);
      if (inner == slang::ParameterCategory::DescriptorTableSlot ||
          inner == slang::ParameterCategory::ConstantBuffer ||
          inner == slang::ParameterCategory::ShaderResource ||
          inner == slang::ParameterCategory::SamplerState) {
        return true;
      }
    }
    return false;
  }
  return category == slang::ParameterCategory::DescriptorTableSlot ||
         category == slang::ParameterCategory::ConstantBuffer ||
         category == slang::ParameterCategory::ShaderResource ||
         category == slang::ParameterCategory::SamplerState;
}

ShaderDescriptorKind descriptorKindFromKind(slang::TypeReflection::Kind kind,
                                            const char* parameter_name) {
  switch (kind) {
    case slang::TypeReflection::Kind::ConstantBuffer:
    case slang::TypeReflection::Kind::ParameterBlock:
      return ShaderDescriptorKind::UniformBuffer;
    case slang::TypeReflection::Kind::SamplerState:
      return ShaderDescriptorKind::Sampler;
    case slang::TypeReflection::Kind::Resource:
    case slang::TypeReflection::Kind::TextureBuffer:
      return ShaderDescriptorKind::SampledImage;
    default:
      LOG_FATAL(
          "[SlangCompiler] unsupported Shader resource layout type kind {} "
          "for '{}'",
          static_cast<int>(kind),
          parameter_name != nullptr ? parameter_name : "<unnamed>");
      return ShaderDescriptorKind::UniformBuffer;
  }
}

ShaderDescriptorKind descriptorKindFromParameter(
    slang::VariableLayoutReflection* var) {
  const char* name = var != nullptr ? var->getName() : nullptr;
  if (var != nullptr) {
    slang::TypeLayoutReflection* type_layout = var->getTypeLayout();
    if (type_layout != nullptr) {
      slang::TypeReflection::Kind layout_kind = type_layout->getKind();
      if (layout_kind == slang::TypeReflection::Kind::Array) {
        slang::TypeLayoutReflection* element = type_layout->getElementTypeLayout();
        if (element != nullptr) {
          layout_kind = element->getKind();
        }
      }
      if (layout_kind != slang::TypeReflection::Kind::None &&
          layout_kind != slang::TypeReflection::Kind::Array &&
          layout_kind != slang::TypeReflection::Kind::Struct) {
        return descriptorKindFromKind(layout_kind, name);
      }
    }
  }
  slang::TypeReflection* type = var != nullptr ? var->getType() : nullptr;
  if (type == nullptr) {
    LOG_FATAL("[SlangCompiler] Shader resource layout type is null");
  }
  slang::TypeReflection* unwrapped = type->unwrapArray();
  if (unwrapped == nullptr) {
    LOG_FATAL("[SlangCompiler] Shader resource layout unwrap failed");
  }
  return descriptorKindFromKind(unwrapped->getKind(), name);
}

uint32_t stageMaskFromSlang(SlangStage stage) {
  switch (stage) {
    case SLANG_STAGE_VERTEX:
      return k_shader_stage_vertex;
    case SLANG_STAGE_FRAGMENT:
      return k_shader_stage_fragment;
    case SLANG_STAGE_COMPUTE:
      return k_shader_stage_compute;
    default:
      return k_shader_stage_vertex | k_shader_stage_fragment;
  }
}

ShaderResourceLayout extractShaderResourceLayout(slang::IComponentType* linked) {
  ASSERT(linked);
  Slang::ComPtr<slang::IBlob> diagnostics;
  slang::ProgramLayout* program_layout = linked->getLayout(0, diagnostics.writeRef());
  logDiagnostics("layout diagnostics", diagnostics.get());
  if (program_layout == nullptr) {
    LOG_FATAL("[SlangCompiler] failed to get ProgramLayout from linked program");
  }

  ShaderResourceLayout layout;
  const unsigned param_count = program_layout->getParameterCount();
  for (unsigned i = 0; i < param_count; ++i) {
    slang::VariableLayoutReflection* var = program_layout->getParameterByIndex(i);
    if (!isDescriptorParameter(var)) {
      continue;
    }
    unsigned set = var->getBindingSpace();
    unsigned binding = var->getBindingIndex();
    if (set == static_cast<unsigned>(SLANG_UNKNOWN_SIZE) ||
        binding == static_cast<unsigned>(SLANG_UNKNOWN_SIZE)) {
      const size_t slot_set =
          var->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot);
      const size_t slot_binding =
          var->getOffset(slang::ParameterCategory::DescriptorTableSlot);
      if (slot_set == SLANG_UNKNOWN_SIZE || slot_binding == SLANG_UNKNOWN_SIZE) {
        LOG_FATAL("[SlangCompiler] Shader resource layout has unresolved binding");
      }
      set = static_cast<unsigned>(slot_set);
      binding = static_cast<unsigned>(slot_binding);
    }
    if (set > 1) {
      LOG_FATAL(
          "[SlangCompiler] Shader resource layout set {} is not 0 or 1",
          set);
    }
    ShaderResourceBinding resource{};
    resource.set = set;
    resource.binding = binding;
    resource.kind = descriptorKindFromParameter(var);
    resource.stage_mask = stageMaskFromSlang(var->getStage());
    if (layout.count >= k_max_expected_descriptor_bindings) {
      LOG_FATAL(
          "[SlangCompiler] Shader resource layout exceeds {} descriptor "
          "bindings",
          k_max_expected_descriptor_bindings);
    }
    layout.bindings[layout.count++] = resource;
  }
  return layout;
}

SlangCompiler::ShaderResult copyEntryPointSpirv(slang::IComponentType* linked,
                                                SlangInt entry_index,
                                                const char* entry_name) {
  Slang::ComPtr<slang::IBlob> diagnostics;
  Slang::ComPtr<slang::IBlob> spirv_blob;
  const SlangResult result = linked->getEntryPointCode(
      entry_index, 0, spirv_blob.writeRef(), diagnostics.writeRef());
  logDiagnostics("code generation diagnostics", diagnostics.get());
  if (SLANG_FAILED(result) || !spirv_blob) {
    LOG_FATAL("[SlangCompiler] failed to generate SPIR-V for entry '{}'",
              entry_name);
  }

  SlangCompiler::ShaderResult shader_result;
  shader_result.entry_point_name =
      eastl::string(entry_name, std::strlen(entry_name));
  const size_t spirv_size = spirv_blob->getBufferSize();
  shader_result.spirv_code.resize(spirv_size);
  std::memcpy(shader_result.spirv_code.data(), spirv_blob->getBufferPointer(),
              spirv_size);
  return shader_result;
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

const char* SlangCompiler::buildTag() const {
  ASSERT(m_global_session);
  return slangBuildTag(m_global_session);
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
  (void)stage;
  ASSERT(m_global_session);
  ASSERT(source_path);
  ASSERT(entry_point);

  eastl::string source_code = readFileToString(source_path);

  m_last_bytecode_hit = false;
  uint8_t source_hash[32];
  hashSourceBytes(source_code, source_hash);
  const char* slang_tag = slangBuildTag(m_global_session);
  CachedShaderSpirv cached;
  if (tryLoadShaderBytecode(source_hash, slang_tag, k_spirv_profile_name,
                            entry_point, &cached)) {
    m_last_bytecode_hit = true;
    ShaderResult shader_result;
    shader_result.entry_point_name =
        eastl::string(entry_point, std::strlen(entry_point));
    shader_result.spirv_code = cached.spirv;
    LOG_INFO(
        "[SlangCompiler] bytecode cache hit '{}' entry '{}' -> {} bytes SPIR-V",
        source_path, entry_point, shader_result.spirv_code.size());
    return shader_result;
  }
  LOG_INFO("[SlangCompiler] bytecode cache miss '{}' entry '{}'", source_path,
           entry_point);

  slang::TargetDesc target_desc{};
  target_desc.format = SLANG_SPIRV;
  target_desc.profile = m_global_session->findProfile(k_spirv_profile_name);

  slang::CompilerOptionEntry options[] = {
      {slang::CompilerOptionName::EmitSpirvDirectly,
       {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
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

  Slang::ComPtr<slang::IBlob> diagnostics_blob;
  Slang::ComPtr<slang::IModule> shader_module(
      session->loadModuleFromSourceString("shader", source_path,
                                          source_code.c_str(),
                                          diagnostics_blob.writeRef()));
  logDiagnostics("compilation diagnostics", diagnostics_blob.get());
  if (!shader_module) {
    LOG_FATAL("[SlangCompiler::compileShader] failed to load module from: {}",
              source_path);
  }

  Slang::ComPtr<slang::IEntryPoint> entry_point_obj;
  result = shader_module->findEntryPointByName(entry_point,
                                               entry_point_obj.writeRef());
  if (SLANG_FAILED(result) || !entry_point_obj) {
    LOG_FATAL(
        "[SlangCompiler::compileShader] entry point '{}' not found in: {}",
        entry_point, source_path);
  }

  slang::IComponentType* components[] = {shader_module, entry_point_obj};
  Slang::ComPtr<slang::IComponentType> composed_program;
  diagnostics_blob = nullptr;
  result = session->createCompositeComponentType(
      components, 2, composed_program.writeRef(), diagnostics_blob.writeRef());
  logDiagnostics("composition diagnostics", diagnostics_blob.get());
  if (SLANG_FAILED(result) || !composed_program) {
    LOG_FATAL(
        "[SlangCompiler::compileShader] failed to compose program for: {}",
        source_path);
  }

  Slang::ComPtr<slang::IComponentType> linked_program;
  diagnostics_blob = nullptr;
  result = composed_program->link(linked_program.writeRef(),
                                  diagnostics_blob.writeRef());
  logDiagnostics("link diagnostics", diagnostics_blob.get());
  if (SLANG_FAILED(result) || !linked_program) {
    LOG_FATAL("[SlangCompiler::compileShader] failed to link program for: {}",
              source_path);
  }

  ShaderResult shader_result =
      copyEntryPointSpirv(linked_program, 0, entry_point);
  CachedShaderSpirv stored;
  stored.spirv = shader_result.spirv_code;
  tryStoreShaderBytecode(source_hash, slang_tag, k_spirv_profile_name,
                         entry_point, stored);
  LOG_INFO("[SlangCompiler] compiled '{}' entry '{}' -> {} bytes SPIR-V",
           source_path, entry_point, shader_result.spirv_code.size());
  return shader_result;
}

SlangCompiler::GraphicsProgramResult SlangCompiler::compileGraphicsProgram(
    const char* source_path, const char* vertex_entry,
    const char* fragment_entry) {
  ASSERT(m_global_session);
  ASSERT(source_path);
  ASSERT(vertex_entry);
  ASSERT(fragment_entry);

  eastl::string source_code = readFileToString(source_path);

  m_last_bytecode_hit = false;
  uint8_t source_hash[32];
  hashSourceBytes(source_code, source_hash);
  const char* slang_tag = slangBuildTag(m_global_session);
  CachedGraphicsProgram cached;
  if (tryLoadGraphicsBytecode(source_hash, slang_tag, k_spirv_profile_name,
                              vertex_entry, fragment_entry, &cached)) {
    m_last_bytecode_hit = true;
    GraphicsProgramResult program;
    program.vertex.entry_point_name =
        eastl::string(vertex_entry, std::strlen(vertex_entry));
    program.vertex.spirv_code = cached.vertex_spirv;
    program.fragment.entry_point_name =
        eastl::string(fragment_entry, std::strlen(fragment_entry));
    program.fragment.spirv_code = cached.fragment_spirv;
    program.layout = cached.layout;
    LOG_INFO(
        "[SlangCompiler] bytecode cache hit graphics '{}' VS {} bytes FS {} "
        "bytes, {} descriptor bindings",
        source_path, program.vertex.spirv_code.size(),
        program.fragment.spirv_code.size(), program.layout.count);
    return program;
  }
  LOG_INFO("[SlangCompiler] bytecode cache miss graphics '{}'", source_path);

  slang::TargetDesc target_desc{};
  target_desc.format = SLANG_SPIRV;
  target_desc.profile = m_global_session->findProfile(k_spirv_profile_name);

  slang::CompilerOptionEntry options[] = {
      {slang::CompilerOptionName::EmitSpirvDirectly,
       {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
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
    LOG_FATAL(
        "[SlangCompiler::compileGraphicsProgram] failed to create Slang "
        "session");
  }

  Slang::ComPtr<slang::IBlob> diagnostics_blob;
  Slang::ComPtr<slang::IModule> shader_module(
      session->loadModuleFromSourceString("shader", source_path,
                                          source_code.c_str(),
                                          diagnostics_blob.writeRef()));
  logDiagnostics("compilation diagnostics", diagnostics_blob.get());
  if (!shader_module) {
    LOG_FATAL(
        "[SlangCompiler::compileGraphicsProgram] failed to load module from: "
        "{}",
        source_path);
  }

  Slang::ComPtr<slang::IEntryPoint> vertex_obj;
  result = shader_module->findEntryPointByName(vertex_entry,
                                               vertex_obj.writeRef());
  if (SLANG_FAILED(result) || !vertex_obj) {
    LOG_FATAL(
        "[SlangCompiler::compileGraphicsProgram] entry point '{}' not found "
        "in: {}",
        vertex_entry, source_path);
  }

  Slang::ComPtr<slang::IEntryPoint> fragment_obj;
  result = shader_module->findEntryPointByName(fragment_entry,
                                               fragment_obj.writeRef());
  if (SLANG_FAILED(result) || !fragment_obj) {
    LOG_FATAL(
        "[SlangCompiler::compileGraphicsProgram] entry point '{}' not found "
        "in: {}",
        fragment_entry, source_path);
  }

  slang::IComponentType* components[] = {shader_module, vertex_obj,
                                         fragment_obj};
  Slang::ComPtr<slang::IComponentType> composed_program;
  diagnostics_blob = nullptr;
  result = session->createCompositeComponentType(
      components, 3, composed_program.writeRef(), diagnostics_blob.writeRef());
  logDiagnostics("composition diagnostics", diagnostics_blob.get());
  if (SLANG_FAILED(result) || !composed_program) {
    LOG_FATAL(
        "[SlangCompiler::compileGraphicsProgram] failed to compose program "
        "for: {}",
        source_path);
  }

  Slang::ComPtr<slang::IComponentType> linked_program;
  diagnostics_blob = nullptr;
  result = composed_program->link(linked_program.writeRef(),
                                  diagnostics_blob.writeRef());
  logDiagnostics("link diagnostics", diagnostics_blob.get());
  if (SLANG_FAILED(result) || !linked_program) {
    LOG_FATAL(
        "[SlangCompiler::compileGraphicsProgram] failed to link program for: "
        "{}",
        source_path);
  }

  GraphicsProgramResult program;
  program.layout = extractShaderResourceLayout(linked_program);
  program.vertex = copyEntryPointSpirv(linked_program, 0, vertex_entry);
  program.fragment = copyEntryPointSpirv(linked_program, 1, fragment_entry);

  CachedGraphicsProgram stored;
  stored.vertex_spirv = program.vertex.spirv_code;
  stored.fragment_spirv = program.fragment.spirv_code;
  stored.layout = program.layout;
  tryStoreGraphicsBytecode(source_hash, slang_tag, k_spirv_profile_name,
                           vertex_entry, fragment_entry, stored);

  LOG_INFO(
      "[SlangCompiler] compiled graphics '{}' VS {} bytes FS {} bytes, {} "
      "descriptor bindings",
      source_path, program.vertex.spirv_code.size(),
      program.fragment.spirv_code.size(), program.layout.count);
  return program;
}

SlangCompiler::ShaderResult SlangCompiler::compileShaderDxil(
    const char* source_path, const char* entry_point, int stage) {
  ASSERT(m_global_session);
  ASSERT(source_path);
  ASSERT(entry_point);
  (void)stage;

  eastl::string source_code = readFileToString(source_path);

  slang::TargetDesc target_desc{};
  target_desc.format = SLANG_DXIL;
  target_desc.profile = m_global_session->findProfile("sm_6_0");

  slang::SessionDesc session_desc{};
  session_desc.targets = &target_desc;
  session_desc.targetCount = 1;
  session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

  Slang::ComPtr<slang::ISession> session;
  SlangResult result =
      m_global_session->createSession(session_desc, session.writeRef());
  if (SLANG_FAILED(result) || !session) {
    LOG_FATAL(
        "[SlangCompiler::compileShaderDxil] failed to create Slang session");
  }

  Slang::ComPtr<slang::IBlob> diagnostics_blob;
  Slang::ComPtr<slang::IModule> shader_module(session->loadModuleFromSourceString(
      "shader", source_path, source_code.c_str(), diagnostics_blob.writeRef()));

  if (!shader_module) {
    LOG_FATAL("[SlangCompiler::compileShaderDxil] failed to load module: {}",
              source_path);
  }

  Slang::ComPtr<slang::IEntryPoint> entry_point_obj;
  result = shader_module->findEntryPointByName(entry_point,
                                               entry_point_obj.writeRef());
  if (SLANG_FAILED(result) || !entry_point_obj) {
    LOG_FATAL("[SlangCompiler::compileShaderDxil] entry point '{}' not found",
              entry_point);
  }

  slang::IComponentType* components[] = {shader_module, entry_point_obj};
  Slang::ComPtr<slang::IComponentType> composed_program;
  result = session->createCompositeComponentType(
      components, 2, composed_program.writeRef(), diagnostics_blob.writeRef());
  if (SLANG_FAILED(result) || !composed_program) {
    LOG_FATAL("[SlangCompiler::compileShaderDxil] failed to compose program");
  }

  Slang::ComPtr<slang::IComponentType> linked_program;
  result = composed_program->link(linked_program.writeRef(),
                                  diagnostics_blob.writeRef());
  if (SLANG_FAILED(result) || !linked_program) {
    LOG_FATAL("[SlangCompiler::compileShaderDxil] failed to link program");
  }

  Slang::ComPtr<slang::IBlob> dxil_blob;
  result = linked_program->getEntryPointCode(0, 0, dxil_blob.writeRef(),
                                             diagnostics_blob.writeRef());
  if (SLANG_FAILED(result) || !dxil_blob) {
    LOG_FATAL("[SlangCompiler::compileShaderDxil] failed to generate DXIL");
  }

  ShaderResult shader_result;
  shader_result.entry_point_name =
      eastl::string(entry_point, std::strlen(entry_point));
  const size_t dxil_size = dxil_blob->getBufferSize();
  shader_result.dxil_code.resize(dxil_size);
  std::memcpy(shader_result.dxil_code.data(), dxil_blob->getBufferPointer(),
              dxil_size);

  LOG_INFO("[SlangCompiler] compiled '{}' entry '{}' -> {} bytes DXIL",
           source_path, entry_point, dxil_size);
  return shader_result;
}

}  // namespace Blunder
