#include "runtime/function/render/slang/engine_gpu_cache.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "runtime/core/base/macro.h"
#include "runtime/function/render/slang/sha256.h"

namespace Blunder {

namespace {

constexpr uint32_t k_bytecode_magic = 0x31434742u;  // 'BGC1' LE
constexpr size_t k_max_cache_file_bytes = 64u * 1024u * 1024u;

std::filesystem::path cacheRootFromKnownFolders() {
#if defined(_WIN32)
  const char* local = std::getenv("LOCALAPPDATA");
  if (local != nullptr && local[0] != '\0') {
    return std::filesystem::path(local) / "Blunder" / "gpu-cache";
  }
  return std::filesystem::path("Blunder") / "gpu-cache";
#else
  const char* xdg = std::getenv("XDG_CACHE_HOME");
  if (xdg != nullptr && xdg[0] != '\0') {
    return std::filesystem::path(xdg) / "Blunder" / "gpu-cache";
  }
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return std::filesystem::path(home) / ".cache" / "Blunder" / "gpu-cache";
  }
  return std::filesystem::path("Blunder") / "gpu-cache";
#endif
}

void appendU32(eastl::vector<uint8_t>& out, uint32_t value) {
  out.push_back(static_cast<uint8_t>(value));
  out.push_back(static_cast<uint8_t>(value >> 8u));
  out.push_back(static_cast<uint8_t>(value >> 16u));
  out.push_back(static_cast<uint8_t>(value >> 24u));
}

void appendBytes(eastl::vector<uint8_t>& out, const void* data, size_t size) {
  if (size == 0 || data == nullptr) {
    return;
  }
  const size_t old = out.size();
  out.resize(old + size);
  std::memcpy(out.data() + old, data, size);
}

void appendString(eastl::vector<uint8_t>& out, const char* text) {
  const uint32_t len =
      text == nullptr ? 0u : static_cast<uint32_t>(std::strlen(text));
  appendU32(out, len);
  if (len != 0u) {
    appendBytes(out, text, len);
  }
}

bool readU32(const uint8_t*& cursor, const uint8_t* end, uint32_t* value) {
  if (cursor + 4 > end) {
    return false;
  }
  *value = static_cast<uint32_t>(cursor[0]) |
           (static_cast<uint32_t>(cursor[1]) << 8u) |
           (static_cast<uint32_t>(cursor[2]) << 16u) |
           (static_cast<uint32_t>(cursor[3]) << 24u);
  cursor += 4;
  return true;
}

bool readBytes(const uint8_t*& cursor, const uint8_t* end, size_t size,
               uint8_t* dest) {
  if (cursor + size > end) {
    return false;
  }
  std::memcpy(dest, cursor, size);
  cursor += size;
  return true;
}

bool readString(const uint8_t*& cursor, const uint8_t* end,
                eastl::vector<char>* text) {
  uint32_t len = 0;
  if (!readU32(cursor, end, &len)) {
    return false;
  }
  if (cursor + len > end) {
    return false;
  }
  text->assign(reinterpret_cast<const char*>(cursor),
               reinterpret_cast<const char*>(cursor) + len);
  text->push_back('\0');
  cursor += len;
  return true;
}

bool stringsEqual(const eastl::vector<char>& stored, const char* expected) {
  const char* got = stored.empty() ? "" : stored.data();
  const char* want = expected == nullptr ? "" : expected;
  return std::strcmp(got, want) == 0;
}

void hex32(const uint8_t bytes[32], char out[65]) {
  static const char k_hex[] = "0123456789abcdef";
  for (uint32_t i = 0; i < 32u; ++i) {
    out[i * 2u] = k_hex[(bytes[i] >> 4u) & 0xfu];
    out[i * 2u + 1u] = k_hex[bytes[i] & 0xfu];
  }
  out[64] = '\0';
}

void uuidHex(const uint8_t uuid[16], char out[33]) {
  static const char k_hex[] = "0123456789abcdef";
  for (uint32_t i = 0; i < 16u; ++i) {
    out[i * 2u] = k_hex[(uuid[i] >> 4u) & 0xfu];
    out[i * 2u + 1u] = k_hex[uuid[i] & 0xfu];
  }
  out[32] = '\0';
}

std::filesystem::path bytecodeDir() {
  return engineGpuCacheDirectory() / "bytecode";
}

std::filesystem::path pipelinesDir() {
  return engineGpuCacheDirectory() / "pipelines";
}

void hashCompileIdentity(const char* slang_tag, const char* spirv_profile,
                         uint8_t out[32]) {
  const char* tag = slang_tag != nullptr ? slang_tag : "";
  const char* profile = spirv_profile != nullptr ? spirv_profile : "";
  eastl::vector<uint8_t> bytes;
  appendBytes(bytes, tag, std::strlen(tag) + 1u);
  appendBytes(bytes, profile, std::strlen(profile) + 1u);
  sha256(bytes.data(), bytes.size(), out);
}

std::filesystem::path bytecodePath(const uint8_t source_sha256[32],
                                   const char* slang_tag,
                                   const char* spirv_profile,
                                   const char* vertex_entry,
                                   const char* fragment_entry) {
  char hex[65];
  hex32(source_sha256, hex);
  uint8_t ident[32];
  hashCompileIdentity(slang_tag, spirv_profile, ident);
  char ident_hex[65];
  hex32(ident, ident_hex);
  std::filesystem::path name(hex);
  name += "_";
  name += ident_hex;
  name += "_";
  name += (vertex_entry != nullptr ? vertex_entry : "");
  if (fragment_entry != nullptr && fragment_entry[0] != '\0') {
    name += "_";
    name += fragment_entry;
  }
  name += ".bin";
  return bytecodeDir() / name;
}

std::filesystem::path pipelinePath(const uint8_t uuid[16],
                                   const char* slang_tag) {
  char hex[33];
  uuidHex(uuid, hex);
  uint8_t ident[32];
  hashCompileIdentity(slang_tag, k_spirv_profile_name, ident);
  char ident_hex[65];
  hex32(ident, ident_hex);
  std::string name(hex);
  name += "_g";
  name += std::to_string(k_engine_gpu_cache_generation);
  name += "_";
  name += ident_hex;
  name += ".bin";
  return pipelinesDir() / name;
}

bool readWholeFile(const std::filesystem::path& path,
                   eastl::vector<uint8_t>* out) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) {
    return false;
  }
  const auto size = file.tellg();
  if (size <= 0) {
    return false;
  }
  if (static_cast<size_t>(size) > k_max_cache_file_bytes) {
    LOG_WARN("[EngineGpuCache] refusing oversized cache file '{}'",
             path.string());
    file.close();
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return false;
  }
  out->resize(static_cast<size_t>(size));
  file.seekg(0);
  file.read(reinterpret_cast<char*>(out->data()), size);
  return static_cast<bool>(file);
}

bool writeAtomic(const std::filesystem::path& path,
                 const eastl::vector<uint8_t>& bytes) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    LOG_WARN("[EngineGpuCache] failed to create directory '{}': {}",
             path.parent_path().string(), ec.message());
    return false;
  }

  std::filesystem::path tmp = path;
  tmp += ".";
#ifdef _WIN32
  tmp += std::to_string(_getpid());
#else
  tmp += std::to_string(getpid());
#endif
  tmp += ".";
  static std::atomic<uint32_t> tmp_seq{0};
  tmp += std::to_string(tmp_seq.fetch_add(1u) + 1u);
  tmp += ".tmp";
  {
    std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
    if (!file) {
      LOG_WARN("[EngineGpuCache] failed to open '{}' for write", tmp.string());
      return false;
    }
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    file.flush();
    if (!file) {
      LOG_WARN("[EngineGpuCache] failed to write '{}'", tmp.string());
      file.close();
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }
  if (!std::filesystem::exists(tmp, ec)) {
    return false;
  }
  {
    std::error_code write_ec;
    const auto written = std::filesystem::file_size(tmp, write_ec);
    if (write_ec || written != bytes.size()) {
      LOG_WARN("[EngineGpuCache] failed to write '{}'", tmp.string());
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }

  std::filesystem::remove(path, ec);
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    LOG_WARN("[EngineGpuCache] failed to replace '{}': {}", path.string(),
             ec.message());
    return false;
  }
  return true;
}

void deleteIfExists(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

void appendLayout(eastl::vector<uint8_t>& out,
                  const ShaderResourceLayout& layout) {
  appendU32(out, layout.count);
  for (uint32_t i = 0; i < layout.count; ++i) {
    const ShaderResourceBinding& b = layout.bindings[i];
    appendU32(out, b.set);
    appendU32(out, b.binding);
    appendU32(out, static_cast<uint32_t>(b.kind));
    appendU32(out, b.stage_mask);
  }
}

bool readLayout(const uint8_t*& cursor, const uint8_t* end,
                ShaderResourceLayout* layout) {
  uint32_t count = 0;
  if (!readU32(cursor, end, &count)) {
    return false;
  }
  if (count > k_max_expected_descriptor_bindings) {
    return false;
  }
  layout->count = count;
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t set = 0;
    uint32_t binding = 0;
    uint32_t kind = 0;
    uint32_t stage = 0;
    if (!readU32(cursor, end, &set) || !readU32(cursor, end, &binding) ||
        !readU32(cursor, end, &kind) || !readU32(cursor, end, &stage)) {
      return false;
    }
    layout->bindings[i].set = set;
    layout->bindings[i].binding = binding;
    if (kind > static_cast<uint32_t>(ShaderDescriptorKind::Sampler)) {
      return false;
    }
    layout->bindings[i].kind = static_cast<ShaderDescriptorKind>(kind);
    layout->bindings[i].stage_mask = stage;
  }
  return true;
}

eastl::vector<uint8_t> wrapPayload(const eastl::vector<uint8_t>& payload) {
  eastl::vector<uint8_t> file;
  file.reserve(8u + 32u + payload.size());
  appendU32(file, k_bytecode_magic);
  appendU32(file, k_engine_gpu_cache_generation);
  uint8_t digest[32];
  sha256(payload.data(), payload.size(), digest);
  appendBytes(file, digest, 32);
  appendBytes(file, payload.data(), payload.size());
  return file;
}

bool unwrapPayload(const eastl::vector<uint8_t>& file,
                   eastl::vector<uint8_t>* payload) {
  if (file.size() < 8u + 32u) {
    return false;
  }
  const uint8_t* cursor = file.data();
  const uint8_t* end = file.data() + file.size();
  uint32_t magic = 0;
  uint32_t generation = 0;
  if (!readU32(cursor, end, &magic) || magic != k_bytecode_magic) {
    return false;
  }
  if (!readU32(cursor, end, &generation) ||
      generation != k_engine_gpu_cache_generation) {
    return false;
  }
  uint8_t stored[32];
  if (!readBytes(cursor, end, 32, stored)) {
    return false;
  }
  uint8_t digest[32];
  sha256(cursor, static_cast<size_t>(end - cursor), digest);
  if (std::memcmp(stored, digest, 32) != 0) {
    return false;
  }
  payload->assign(cursor, end);
  return true;
}

eastl::vector<uint8_t> encodeProgram(const uint8_t source_sha256[32],
                                     const char* slang_tag,
                                     const char* spirv_profile,
                                     const char* vertex_entry,
                                     const char* fragment_entry,
                                     const CachedGraphicsProgram& program) {
  eastl::vector<uint8_t> payload;
  appendString(payload, slang_tag);
  appendString(payload, spirv_profile);
  appendBytes(payload, source_sha256, 32);
  appendString(payload, vertex_entry);
  appendString(payload, fragment_entry);
  appendU32(payload, static_cast<uint32_t>(program.vertex_spirv.size()));
  appendBytes(payload, program.vertex_spirv.data(),
              program.vertex_spirv.size());
  appendU32(payload, static_cast<uint32_t>(program.fragment_spirv.size()));
  appendBytes(payload, program.fragment_spirv.data(),
              program.fragment_spirv.size());
  appendLayout(payload, program.layout);
  return wrapPayload(payload);
}

bool decodeProgram(const eastl::vector<uint8_t>& payload,
                   const uint8_t source_sha256[32], const char* slang_tag,
                   const char* spirv_profile, const char* vertex_entry,
                   const char* fragment_entry, CachedGraphicsProgram* out) {
  const uint8_t* cursor = payload.data();
  const uint8_t* end = payload.data() + payload.size();
  eastl::vector<char> tag;
  eastl::vector<char> profile;
  eastl::vector<char> vs;
  eastl::vector<char> fs;
  uint8_t stored_hash[32];
  if (!readString(cursor, end, &tag) || !stringsEqual(tag, slang_tag) ||
      !readString(cursor, end, &profile) ||
      !stringsEqual(profile, spirv_profile) ||
      !readBytes(cursor, end, 32, stored_hash) ||
      std::memcmp(stored_hash, source_sha256, 32) != 0 ||
      !readString(cursor, end, &vs) || !stringsEqual(vs, vertex_entry) ||
      !readString(cursor, end, &fs) || !stringsEqual(fs, fragment_entry)) {
    return false;
  }
  uint32_t vs_size = 0;
  uint32_t fs_size = 0;
  if (!readU32(cursor, end, &vs_size) || (vs_size % 4u) != 0u ||
      cursor + vs_size > end) {
    return false;
  }
  out->vertex_spirv.assign(cursor, cursor + vs_size);
  cursor += vs_size;
  if (!readU32(cursor, end, &fs_size) || (fs_size % 4u) != 0u ||
      cursor + fs_size > end) {
    return false;
  }
  out->fragment_spirv.assign(cursor, cursor + fs_size);
  cursor += fs_size;
  if (!readLayout(cursor, end, &out->layout) || cursor != end) {
    return false;
  }
  return true;
}

}  // namespace

std::filesystem::path defaultEngineGpuCacheDirectory() {
  return cacheRootFromKnownFolders();
}

std::filesystem::path engineGpuCacheDirectory() {
  const char* override_dir = std::getenv(k_gpu_cache_dir_env);
  if (override_dir != nullptr && override_dir[0] != '\0') {
    const std::filesystem::path path(override_dir);
    if (path.is_absolute()) {
      return path;
    }
    LOG_WARN(
        "[EngineGpuCache] {} is not absolute; using default Engine GPU cache",
        k_gpu_cache_dir_env);
  }
  return defaultEngineGpuCacheDirectory();
}

bool tryLoadGraphicsBytecode(const uint8_t source_sha256[32],
                             const char* slang_tag, const char* spirv_profile,
                             const char* vertex_entry, const char* fragment_entry,
                             CachedGraphicsProgram* out) {
  const std::filesystem::path path =
      bytecodePath(source_sha256, slang_tag, spirv_profile, vertex_entry,
                   fragment_entry);
  eastl::vector<uint8_t> file;
  if (!readWholeFile(path, &file)) {
    return false;
  }
  eastl::vector<uint8_t> payload;
  if (!unwrapPayload(file, &payload)) {
    LOG_WARN("[EngineGpuCache] discarding corrupt bytecode '{}'",
             path.string());
    deleteIfExists(path);
    return false;
  }
  if (!decodeProgram(payload, source_sha256, slang_tag, spirv_profile,
                     vertex_entry, fragment_entry, out)) {
    return false;
  }
  return true;
}

void tryStoreGraphicsBytecode(const uint8_t source_sha256[32],
                              const char* slang_tag, const char* spirv_profile,
                              const char* vertex_entry, const char* fragment_entry,
                              const CachedGraphicsProgram& program) {
  const eastl::vector<uint8_t> file = encodeProgram(
      source_sha256, slang_tag, spirv_profile, vertex_entry, fragment_entry,
      program);
  const std::filesystem::path path =
      bytecodePath(source_sha256, slang_tag, spirv_profile, vertex_entry,
                   fragment_entry);
  if (!writeAtomic(path, file)) {
    LOG_WARN("[EngineGpuCache] bytecode write skipped for '{}'", path.string());
  }
}

bool tryLoadShaderBytecode(const uint8_t source_sha256[32],
                           const char* slang_tag, const char* spirv_profile,
                           const char* entry_point, CachedShaderSpirv* out) {
  CachedGraphicsProgram program;
  if (!tryLoadGraphicsBytecode(source_sha256, slang_tag, spirv_profile,
                               entry_point, "", &program)) {
    return false;
  }
  out->spirv = program.vertex_spirv;
  return !out->spirv.empty();
}

void tryStoreShaderBytecode(const uint8_t source_sha256[32],
                            const char* slang_tag, const char* spirv_profile,
                            const char* entry_point,
                            const CachedShaderSpirv& shader) {
  CachedGraphicsProgram program;
  program.vertex_spirv = shader.spirv;
  tryStoreGraphicsBytecode(source_sha256, slang_tag, spirv_profile, entry_point,
                           "", program);
}

eastl::vector<uint8_t> tryLoadPipelineCacheBlob(const uint8_t uuid[16],
                                                const char* slang_tag) {
  eastl::vector<uint8_t> file;
  const std::filesystem::path path = pipelinePath(uuid, slang_tag);
  if (!readWholeFile(path, &file)) {
    return {};
  }
  return file;
}

void tryStorePipelineCacheBlob(const uint8_t uuid[16], const char* slang_tag,
                               const void* data, size_t size) {
  if (data == nullptr || size == 0) {
    return;
  }
  eastl::vector<uint8_t> bytes;
  bytes.resize(size);
  std::memcpy(bytes.data(), data, size);
  const std::filesystem::path path = pipelinePath(uuid, slang_tag);
  if (!writeAtomic(path, bytes)) {
    LOG_WARN("[EngineGpuCache] Pipeline cache write skipped for '{}'",
             path.string());
  }
}

void deletePipelineCacheBlob(const uint8_t uuid[16], const char* slang_tag) {
  deleteIfExists(pipelinePath(uuid, slang_tag));
}

}  // namespace Blunder
