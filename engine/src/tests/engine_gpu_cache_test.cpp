#include "runtime/core/log/log_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/slang/engine_gpu_cache.h"
#include "runtime/function/render/slang/sha256.h"
#include "runtime/function/render/slang/slang_compiler.h"

#include "EASTL/shared_ptr.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

void setCacheDir(const std::string& dir) {
#ifdef _WIN32
  _putenv_s(Blunder::k_gpu_cache_dir_env, dir.c_str());
#else
  setenv(Blunder::k_gpu_cache_dir_env, dir.c_str(), 1);
#endif
}

bool hashFile(const std::filesystem::path& path, uint8_t out[32]) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::string bytes((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
  Blunder::sha256(bytes.data(), bytes.size(), out);
  return true;
}

std::filesystem::path firstBytecodeBin(const std::filesystem::path& cache_root) {
  const std::filesystem::path dir = cache_root / "bytecode";
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) {
    return {};
  }
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (entry.is_regular_file() && entry.path().extension() == ".bin") {
      return entry.path();
    }
  }
  return {};
}

bool copyFile(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::ifstream in(from, std::ios::binary);
  if (!in) {
    return false;
  }
  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << in.rdbuf();
  return static_cast<bool>(out);
}

bool appendBytes(const std::filesystem::path& path, const char* extra) {
  std::ofstream out(path, std::ios::binary | std::ios::app);
  if (!out) {
    return false;
  }
  out << extra;
  return static_cast<bool>(out);
}

bool corruptAllBytecodeFiles(const std::filesystem::path& cache_root) {
  const std::filesystem::path dir = cache_root / "bytecode";
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) {
    return false;
  }
  bool any = false;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".bin") {
      continue;
    }
    std::fstream file(entry.path(),
                      std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
      continue;
    }
    char byte = 0;
    file.read(&byte, 1);
    if (!file) {
      continue;
    }
    byte = static_cast<char>(static_cast<unsigned char>(byte) ^ 0xffu);
    file.seekp(0);
    file.write(&byte, 1);
    if (file) {
      any = true;
    }
  }
  return any;
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  {
    uint8_t digest[32];
    const char abc[] = "abc";
    sha256(abc, 3, digest);
    const uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde,
        0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    expect_true("sha256 abc", std::memcmp(digest, expected, 32) == 0);
  }

  {
    const std::filesystem::path editor = defaultEngineGpuCacheDirectory();
    const std::filesystem::path player = defaultEngineGpuCacheDirectory();
    expect_true("default Engine GPU cache path identical for Editor/Player",
                editor == player);
    const std::string as_text = editor.generic_string();
    expect_true("default cache is not under .blunder",
                as_text.find(".blunder") == std::string::npos);
  }

  std::filesystem::path cache_root = std::filesystem::temp_directory_path() /
                                     "blunder-gpu-cache-test";
#ifdef _WIN32
  cache_root += std::to_string(_getpid());
#else
  cache_root += std::to_string(getpid());
#endif
  std::error_code ec;
  std::filesystem::remove_all(cache_root, ec);
  std::filesystem::create_directories(cache_root, ec);
  setCacheDir(cache_root.string());
  expect_true("env override is used",
              engineGpuCacheDirectory() == cache_root);

  {
    const std::filesystem::path def = defaultEngineGpuCacheDirectory();
    setCacheDir("relative-gpu-cache");
    expect_true("non-absolute override ignored",
                engineGpuCacheDirectory() == def);
    setCacheDir("");
    expect_true("empty override ignored", engineGpuCacheDirectory() == def);
    setCacheDir(cache_root.string());
  }

  const std::filesystem::path scratch_slang = cache_root / "grid_copy.slang";
  expect_true("copy grid.slang to temp",
              copyFile("engine/shaders/grid.slang", scratch_slang));

  SlangCompiler compiler;
  compiler.initialize();

  {
    const auto first =
        compiler.compileGraphicsProgram(scratch_slang.string().c_str());
    expect_true("first compile is miss", !compiler.lastBytecodeCacheHit());
    expect_true("first compile has SPIR-V",
                !first.vertex.spirv_code.empty() &&
                    !first.fragment.spirv_code.empty());
    const uint32_t layout_count = first.layout.count;

    const auto second =
        compiler.compileGraphicsProgram(scratch_slang.string().c_str());
    expect_true("second compile is bytecode hit",
                compiler.lastBytecodeCacheHit());
    expect_true("hit restores same SPIR-V sizes",
                second.vertex.spirv_code.size() == first.vertex.spirv_code.size() &&
                    second.fragment.spirv_code.size() ==
                        first.fragment.spirv_code.size());
    expect_true("hit restores layout count",
                second.layout.count == layout_count);
  }

  {
    compiler.compileShader(scratch_slang.string().c_str(), "vertexMain", 0);
    expect_true("compileShader first is miss", !compiler.lastBytecodeCacheHit());
    compiler.compileShader(scratch_slang.string().c_str(), "vertexMain", 0);
    expect_true("compileShader second is bytecode hit",
                compiler.lastBytecodeCacheHit());
  }

  {
    uint8_t scratch_hash[32];
    expect_true("hash scratch slang", hashFile(scratch_slang, scratch_hash));
    CachedGraphicsProgram wrong{};
    expect_true(
        "slang tag mismatch misses",
        !tryLoadGraphicsBytecode(scratch_hash, "not-this-tag",
                                 k_spirv_profile_name, "vertexMain",
                                 "fragmentMain", &wrong));
    expect_true("tag mismatch keeps bytecode file",
                !firstBytecodeBin(cache_root).empty());
  }

  {
    uint8_t uuid[16]{};
    uuid[0] = 1;
    const uint8_t blob[] = {9, 8, 7, 6};
    tryStorePipelineCacheBlob(uuid, "tag-a", blob, sizeof(blob));
    const eastl::vector<uint8_t> got =
        tryLoadPipelineCacheBlob(uuid, "tag-a");
    expect_true("pipeline blob roundtrip",
                got.size() == sizeof(blob) && got[0] == 9);
    expect_true("pipeline blob misses other slang tag",
                tryLoadPipelineCacheBlob(uuid, "tag-b").empty());
    tryStorePipelineCacheBlob(uuid, "tag-a", nullptr, 0);
    expect_true("null pipeline store is no-op",
                tryLoadPipelineCacheBlob(uuid, "tag-a").size() == sizeof(blob));
    deletePipelineCacheBlob(uuid, "tag-a");
    expect_true("pipeline blob delete removes file",
                tryLoadPipelineCacheBlob(uuid, "tag-a").empty());
  }

  {
    std::filesystem::path graphics_bin;
    {
      const std::filesystem::path dir = cache_root / "bytecode";
      std::error_code scan_ec;
      for (const auto& entry : std::filesystem::directory_iterator(dir, scan_ec)) {
        if (!entry.is_regular_file()) {
          continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.find("fragmentMain") != std::string::npos) {
          graphics_bin = entry.path();
          break;
        }
      }
    }
    expect_true("graphics bytecode bin exists before digest corrupt",
                !graphics_bin.empty());
    if (!graphics_bin.empty()) {
      std::fstream file(graphics_bin,
                        std::ios::in | std::ios::out | std::ios::binary);
      expect_true("open bytecode for digest corrupt", static_cast<bool>(file));
      if (file) {
        file.seekg(40);
        char byte = 0;
        file.read(&byte, 1);
        file.seekp(40);
        byte = static_cast<char>(static_cast<unsigned char>(byte) ^ 0xffu);
        file.write(&byte, 1);
        file.close();
      }
      uint8_t scratch_hash[32];
      hashFile(scratch_slang, scratch_hash);
      CachedGraphicsProgram out{};
      expect_true("digest mismatch misses",
                  !tryLoadGraphicsBytecode(scratch_hash, compiler.buildTag(),
                                           k_spirv_profile_name, "vertexMain",
                                           "fragmentMain", &out));
      expect_true("corrupt blob deleted",
                  !std::filesystem::exists(graphics_bin));
    }
  }

  {
    expect_true("touch shader source",
                appendBytes(scratch_slang, "\n// cache-bust\n"));
    compiler.compileGraphicsProgram(scratch_slang.string().c_str());
    expect_true("source change is bytecode miss",
                !compiler.lastBytecodeCacheHit());
  }

  {
    compiler.compileGraphicsProgram("engine/shaders/pbr.slang");
    expect_true("corrupt bytecode files",
                corruptAllBytecodeFiles(cache_root));
    const auto recovered =
        compiler.compileGraphicsProgram("engine/shaders/pbr.slang");
    expect_true("corrupt bytecode is miss and still compiles",
                !compiler.lastBytecodeCacheHit() &&
                    !recovered.vertex.spirv_code.empty());
  }

  compiler.shutdown();
  g_runtime_global_context.m_logger_system.reset();
  std::filesystem::remove_all(cache_root, ec);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
