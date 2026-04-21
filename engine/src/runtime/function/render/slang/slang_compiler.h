#pragma once

#include <cstddef>
#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

// Forward declarations for Slang COM types (avoid leaking slang.h to callers)
namespace slang {
struct IGlobalSession;
}  // namespace slang

namespace Blunder {

/// Compiles .slang shader source files to SPIR-V at runtime using the Slang API.
///
/// Lifetime: create once at engine startup, reuse for all shader compilations.
/// The global session is shared and caches compiled modules for performance.
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
    eastl::string entry_point_name;
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

 private:
  slang::IGlobalSession* m_global_session{nullptr};
};

}  // namespace Blunder
