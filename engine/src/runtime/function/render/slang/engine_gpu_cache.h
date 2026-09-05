#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include "EASTL/vector.h"

#include "runtime/function/render/slang/shader_resource_layout.h"

namespace Blunder {

constexpr uint32_t k_engine_gpu_cache_generation = 1;
constexpr const char* k_spirv_profile_name = "spirv_1_5";
constexpr const char* k_gpu_cache_dir_env = "BLUNDER_GPU_CACHE_DIR";

/// Default user-level Engine GPU cache directory (ignores the env override).
std::filesystem::path defaultEngineGpuCacheDirectory();

/// Effective cache root: absolute `BLUNDER_GPU_CACHE_DIR` if set, else default.
std::filesystem::path engineGpuCacheDirectory();

struct CachedGraphicsProgram {
  eastl::vector<uint8_t> vertex_spirv;
  eastl::vector<uint8_t> fragment_spirv;
  ShaderResourceLayout layout;
};

/// Single-entry SPIR-V stored as a graphics blob with an empty fragment entry.
struct CachedShaderSpirv {
  eastl::vector<uint8_t> spirv;
};

bool tryLoadGraphicsBytecode(const uint8_t source_sha256[32],
                             const char* slang_tag, const char* spirv_profile,
                             const char* vertex_entry, const char* fragment_entry,
                             CachedGraphicsProgram* out);

void tryStoreGraphicsBytecode(const uint8_t source_sha256[32],
                              const char* slang_tag, const char* spirv_profile,
                              const char* vertex_entry, const char* fragment_entry,
                              const CachedGraphicsProgram& program);

bool tryLoadShaderBytecode(const uint8_t source_sha256[32],
                           const char* slang_tag, const char* spirv_profile,
                           const char* entry_point, CachedShaderSpirv* out);

void tryStoreShaderBytecode(const uint8_t source_sha256[32],
                            const char* slang_tag, const char* spirv_profile,
                            const char* entry_point,
                            const CachedShaderSpirv& shader);

eastl::vector<uint8_t> tryLoadPipelineCacheBlob(const uint8_t uuid[16],
                                                const char* slang_tag);
void tryStorePipelineCacheBlob(const uint8_t uuid[16], const char* slang_tag,
                               const void* data, size_t size);
void deletePipelineCacheBlob(const uint8_t uuid[16], const char* slang_tag);

}  // namespace Blunder
