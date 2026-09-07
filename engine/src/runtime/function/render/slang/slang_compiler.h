#pragma once

#include <cstddef>
#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/render/slang/shader_resource_layout.h"

// Forward declarations for Slang COM types (avoid leaking slang.h to callers)
namespace slang {
struct IGlobalSession;
}  // namespace slang

namespace Blunder {

/// Compiles .slang shader source files to SPIR-V at runtime using the Slang API.
///
/// Lifetime: create once at engine startup, reuse for all shader compilations.
/// The global session reuses in-process Slang modules. That is not Shader
/// bytecode cache (disk).
class SlangCompiler final {
 public:
  SlangCompiler() = default;
  ~SlangCompiler();

  SlangCompiler(const SlangCompiler&) = delete;
  SlangCompiler& operator=(const SlangCompiler&) = delete;

  void initialize();
  void shutdown();

  /// Result of compiling a single entry point from a .slang file.
  struct ShaderResult {
    eastl::vector<uint8_t> spirv_code;
    eastl::vector<uint8_t> dxil_code;
    eastl::string entry_point_name;
  };

  /// Linked VS+FS program: SPIR-V for both stages plus Shader resource layout.
  struct GraphicsProgramResult {
    ShaderResult vertex;
    ShaderResult fragment;
    ShaderResourceLayout layout;
  };

  /// Compile a .slang source file and extract SPIR-V for the given entry point.
  ///
  /// @param source_path   Path to the .slang file (relative or absolute).
  /// @param entry_point   Name of the entry point function (e.g. "vertexMain").
  /// @param stage         Slang stage constant (SLANG_STAGE_VERTEX, etc.).
  /// @return ShaderResult containing the SPIR-V bytecode.
  /// @throws std::runtime_error (via LOG_FATAL) on compilation failure.
  ShaderResult compileShader(const char* source_path,
                             const char* entry_point,
                             int stage);

  /// Compile vertex+fragment from one Slang session/link. Layout comes from
  /// that linked graphics program.
  GraphicsProgramResult compileGraphicsProgram(
      const char* source_path, const char* vertex_entry = "vertexMain",
      const char* fragment_entry = "fragmentMain");

  /// True when the last compileShader / compileGraphicsProgram restored SPIR-V
  /// from Shader bytecode cache (tests).
  bool lastBytecodeCacheHit() const { return m_last_bytecode_hit; }

  /// Slang global-session build tag used as Shader bytecode / Pipeline cache
  /// compile identity. Valid after initialize().
  const char* buildTag() const;

  /// Compile a .slang source file to DXIL for Direct3D 12.
  ShaderResult compileShaderDxil(const char* source_path,
                                 const char* entry_point,
                                 int stage);

 private:
  slang::IGlobalSession* m_global_session{nullptr};
  bool m_last_bytecode_hit{false};
};

}  // namespace Blunder
