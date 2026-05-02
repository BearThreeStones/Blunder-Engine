#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

struct FileSystemInitInfo {
  // Root used to resolve virtual paths like "shaders/triangle.slang".
  // If empty, the FileSystem will fall back to a build-time configured
  // BLUNDER_PROJECT_ROOT define, then to the executable directory.
  std::filesystem::path project_root;

  // Subdirectory (relative to project_root) that contains runtime asset files.
  // Defaults to "assets" - kept as a separate root so we can swap in a packed
  // asset bundle later without changing call sites.
  std::filesystem::path asset_subdir{"assets"};

  // Subdirectory (relative to project_root) that contains shader sources.
  // Defaults to "engine/shaders".
  std::filesystem::path shader_subdir{"engine/shaders"};
};

/// FileSystem is the only place in the engine that touches std::filesystem
/// directly. Everything else (AssetManager, ConfigManager, shader loader, ...)
/// goes through this interface. This keeps path-resolution policy in one spot
/// and makes it trivial to swap in a packaged-asset backend later.
class FileSystem final {
 public:
  FileSystem() = default;
  ~FileSystem() = default;

  FileSystem(const FileSystem&) = delete;
  FileSystem& operator=(const FileSystem&) = delete;

  void initialize(const FileSystemInitInfo& info = {});
  void shutdown();

  // ---- Roots --------------------------------------------------------------

  const std::filesystem::path& getProjectRoot() const { return m_project_root; }
  const std::filesystem::path& getAssetRoot() const { return m_asset_root; }
  const std::filesystem::path& getShaderRoot() const { return m_shader_root; }
  const std::filesystem::path& getExecutableDir() const {
    return m_executable_dir;
  }

  // ---- Path resolution ----------------------------------------------------

  /// Resolve a relative path against the project root. Absolute paths are
  /// returned unchanged. Returned paths are weakly-canonical (no IO error if
  /// the file does not exist yet).
  std::filesystem::path resolve(const std::filesystem::path& relative) const;

  /// Resolve relative to the asset root (i.e. `<project>/assets/<rel>`).
  std::filesystem::path resolveAsset(
      const std::filesystem::path& relative) const;

  /// Resolve relative to the shader root.
  std::filesystem::path resolveShader(
      const std::filesystem::path& relative) const;

  // ---- Queries ------------------------------------------------------------

  bool exists(const std::filesystem::path& path) const;
  bool isFile(const std::filesystem::path& path) const;
  bool isDirectory(const std::filesystem::path& path) const;
  uint64_t fileSize(const std::filesystem::path& path) const;

  /// Non-recursive directory listing. Returns absolute paths. Returns an
  /// empty vector if `path` is not a directory.
  eastl::vector<std::filesystem::path> listDirectory(
      const std::filesystem::path& path) const;

  // ---- Read ---------------------------------------------------------------

  /// Read the entire file as raw bytes. Logs and returns false on failure.
  bool readBinary(const std::filesystem::path& path,
                  eastl::vector<uint8_t>& out_bytes) const;

  /// Read the entire file as text. Logs and returns false on failure.
  bool readText(const std::filesystem::path& path,
                eastl::string& out_text) const;

  // ---- Write --------------------------------------------------------------

  /// Write raw bytes, creating parent directories as needed. Returns false on
  /// failure.
  bool writeBinary(const std::filesystem::path& path, const void* data,
                   size_t size) const;

  bool writeText(const std::filesystem::path& path,
                 const eastl::string& text) const;

 private:
  std::filesystem::path m_executable_dir;
  std::filesystem::path m_project_root;
  std::filesystem::path m_asset_root;
  std::filesystem::path m_shader_root;
  bool m_is_initialized{false};
};

}  // namespace Blunder
