#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

enum class ContentRoot : uint8_t {
  Assets = 0,
  Resources = 1,
  EngineShaders = 2,
};

struct ContentRootInfo {
  ContentRoot root{ContentRoot::Assets};
  const char* display_name{nullptr};
  std::filesystem::path absolute_path;
};

struct DirectoryEntry {
  std::filesystem::path absolute_path;
  eastl::string relative_path;
  bool is_directory{false};
};

struct FileSystemInitInfo {
  // Root used to resolve virtual paths like "shaders/triangle.slang".
  // If empty, the FileSystem will fall back to a build-time configured
  // BLUNDER_PROJECT_ROOT define, then to the executable directory.
  std::filesystem::path project_root;

  // Subdirectory (relative to project_root) for engine asset configuration.
  std::filesystem::path assets_subdir{"Assets"};

  // Subdirectory (relative to project_root) for Intermediate data bodies
  // (glTF, images, …) and optional Source Assets under Source/.
  std::filesystem::path resources_subdir{"Resources"};

  // Subdirectory (relative to project_root) that contains built-in shader sources.
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
  const std::filesystem::path& getResourcesRoot() const { return m_resources_root; }
  const std::filesystem::path& getShaderRoot() const { return m_shader_root; }
  const std::filesystem::path& getExecutableDir() const {
    return m_executable_dir;
  }

  eastl::vector<ContentRootInfo> getContentRoots() const;

  // ---- Path resolution ----------------------------------------------------

  /// Resolve a relative path against the project root. Absolute paths are
  /// returned unchanged.
  std::filesystem::path resolve(const std::filesystem::path& relative) const;

  /// Resolve relative to the Assets root (i.e. `<project>/Assets/<rel>`).
  std::filesystem::path resolveAsset(
      const std::filesystem::path& relative) const;

  /// Resolve relative to the Resources root.
  std::filesystem::path resolveResource(
      const std::filesystem::path& relative) const;

  /// Resolve relative to the shader root.
  std::filesystem::path resolveShader(
      const std::filesystem::path& relative) const;

  // ---- Queries ------------------------------------------------------------

  bool exists(const std::filesystem::path& path) const;
  bool isFile(const std::filesystem::path& path) const;
  bool isDirectory(const std::filesystem::path& path) const;
  uint64_t fileSize(const std::filesystem::path& path) const;
  uint64_t lastWriteTime(const std::filesystem::path& path) const;

  /// Non-recursive directory listing. Returns absolute paths.
  eastl::vector<std::filesystem::path> listDirectory(
      const std::filesystem::path& path) const;

  /// Recursive listing. `max_depth` of -1 walks the full tree; 0 lists only
  /// immediate children of `path`.
  eastl::vector<DirectoryEntry> listDirectoryRecursive(
      const std::filesystem::path& path,
      const std::filesystem::path& relative_root, int32_t max_depth) const;

  // ---- Read ---------------------------------------------------------------

  bool readBinary(const std::filesystem::path& path,
                  eastl::vector<uint8_t>& out_bytes) const;

  bool readText(const std::filesystem::path& path,
                eastl::string& out_text) const;

  // ---- Write --------------------------------------------------------------

  bool writeBinary(const std::filesystem::path& path, const void* data,
                   size_t size) const;

  bool writeText(const std::filesystem::path& path,
                 const eastl::string& text) const;

  /// Creates parent directories for `path` when missing.
  bool ensureParentDirectory(const std::filesystem::path& path) const;

  /// Copies a regular file. When `overwrite` is false and `dst` exists, returns false.
  bool copyFile(const std::filesystem::path& src,
                const std::filesystem::path& dst,
                bool overwrite = false) const;

  /// Renames when possible; otherwise copy + remove for cross-device moves.
  bool movePath(const std::filesystem::path& src,
                const std::filesystem::path& dst) const;

 private:
  void listDirectoryRecursiveImpl(const std::filesystem::path& path,
                                  const std::filesystem::path& relative_root,
                                  int32_t depth_remaining,
                                  eastl::vector<DirectoryEntry>& out) const;

  std::filesystem::path m_executable_dir;
  std::filesystem::path m_project_root;
  std::filesystem::path m_asset_root;
  std::filesystem::path m_resources_root;
  std::filesystem::path m_shader_root;
  bool m_is_initialized{false};
};

}  // namespace Blunder
