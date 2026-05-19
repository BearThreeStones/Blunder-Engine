#include "runtime/platform/file_system/file_system.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <ios>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "runtime/core/base/macro.h"

namespace Blunder {

namespace {

namespace fs = std::filesystem;

fs::path queryExecutablePath() {
#ifdef _WIN32
  wchar_t buffer[MAX_PATH];
  const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (len == 0 || len == MAX_PATH) {
    return {};
  }
  return fs::path(buffer);
#elif defined(__linux__)
  char buffer[4096];
  const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (len <= 0) {
    return {};
  }
  buffer[len] = '\0';
  return fs::path(buffer);
#elif defined(__APPLE__)
  char buffer[4096];
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    return {};
  }
  return fs::path(buffer);
#else
  return {};
#endif
}

}  // namespace

void FileSystem::initialize(const FileSystemInitInfo& info) {
  if (m_is_initialized) {
    return;
  }

  std::error_code ec;

  const fs::path exe_path = queryExecutablePath();
  if (!exe_path.empty()) {
    m_executable_dir = exe_path.parent_path();
  } else {
    m_executable_dir = fs::current_path(ec);
  }

  if (!info.project_root.empty()) {
    m_project_root = info.project_root;
  } else {
#ifdef BLUNDER_PROJECT_ROOT
    m_project_root = fs::path(BLUNDER_PROJECT_ROOT);
#else
    m_project_root = m_executable_dir;
#endif
  }

  // Normalize without requiring existence so the engine still starts in
  // packaged builds where some directories may not exist yet.
  m_project_root = fs::weakly_canonical(m_project_root, ec);
  if (ec) {
    m_project_root = info.project_root.empty() ? m_executable_dir
                                               : info.project_root;
    ec.clear();
  }

  m_asset_root = m_project_root / info.assets_subdir;
  m_resources_root = m_project_root / info.resources_subdir;
  m_shader_root = m_project_root / info.shader_subdir;

  m_is_initialized = true;

  LOG_INFO("[FileSystem] executable dir: {}",
           m_executable_dir.generic_string());
  LOG_INFO("[FileSystem] project root  : {}", m_project_root.generic_string());
  LOG_INFO("[FileSystem] assets root   : {}", m_asset_root.generic_string());
  LOG_INFO("[FileSystem] resources root: {}",
           m_resources_root.generic_string());
  LOG_INFO("[FileSystem] shader root   : {}", m_shader_root.generic_string());
}

void FileSystem::shutdown() {
  m_is_initialized = false;
  m_project_root.clear();
  m_asset_root.clear();
  m_resources_root.clear();
  m_shader_root.clear();
  m_executable_dir.clear();
}

eastl::vector<ContentRootInfo> FileSystem::getContentRoots() const {
  eastl::vector<ContentRootInfo> roots;
  roots.push_back(
      {ContentRoot::Assets, "Assets", m_asset_root});
  roots.push_back(
      {ContentRoot::Resources, "Resources", m_resources_root});
  roots.push_back(
      {ContentRoot::EngineShaders, "Engine Shaders", m_shader_root});
  return roots;
}

fs::path FileSystem::resolve(const fs::path& relative) const {
  if (relative.is_absolute()) {
    return relative;
  }
  return m_project_root / relative;
}

fs::path FileSystem::resolveAsset(const fs::path& relative) const {
  if (relative.is_absolute()) {
    return relative;
  }
  return m_asset_root / relative;
}

fs::path FileSystem::resolveResource(const fs::path& relative) const {
  if (relative.is_absolute()) {
    return relative;
  }
  return m_resources_root / relative;
}

fs::path FileSystem::resolveShader(const fs::path& relative) const {
  if (relative.is_absolute()) {
    return relative;
  }
  return m_shader_root / relative;
}

bool FileSystem::exists(const fs::path& path) const {
  std::error_code ec;
  return fs::exists(path, ec);
}

bool FileSystem::isFile(const fs::path& path) const {
  std::error_code ec;
  return fs::is_regular_file(path, ec);
}

bool FileSystem::isDirectory(const fs::path& path) const {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

uint64_t FileSystem::fileSize(const fs::path& path) const {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec) {
    return 0;
  }
  return static_cast<uint64_t>(size);
}

uint64_t FileSystem::lastWriteTime(const fs::path& path) const {
  std::error_code ec;
  const auto time = fs::last_write_time(path, ec);
  if (ec) {
    return 0;
  }
  return static_cast<uint64_t>(time.time_since_epoch().count());
}

eastl::vector<fs::path> FileSystem::listDirectory(const fs::path& path) const {
  eastl::vector<fs::path> entries;
  std::error_code ec;
  if (!fs::is_directory(path, ec)) {
    return entries;
  }
  for (const auto& entry : fs::directory_iterator(path, ec)) {
    if (ec) {
      break;
    }
    entries.push_back(entry.path());
  }
  if (ec) {
    LOG_WARN("[FileSystem] listDirectory({}) iteration error: {}",
             path.generic_string(), ec.message());
  }
  return entries;
}

void FileSystem::listDirectoryRecursiveImpl(
    const fs::path& path, const fs::path& relative_root, int32_t depth_remaining,
    eastl::vector<DirectoryEntry>& out) const {
  std::error_code ec;
  if (!fs::is_directory(path, ec)) {
    return;
  }

  for (const auto& entry : fs::directory_iterator(path, ec)) {
    if (ec) {
      break;
    }

    const fs::path absolute = entry.path();
    fs::path relative = absolute.lexically_relative(relative_root);
    relative = relative.lexically_normal();

    DirectoryEntry dir_entry{};
    dir_entry.absolute_path = absolute;
    dir_entry.relative_path = eastl::string(relative.generic_string().c_str());
    dir_entry.is_directory = entry.is_directory(ec);
    out.push_back(dir_entry);

    if (dir_entry.is_directory && depth_remaining != 0) {
      const int32_t next_depth =
          depth_remaining < 0 ? depth_remaining : depth_remaining - 1;
      listDirectoryRecursiveImpl(absolute, relative_root, next_depth, out);
    }
  }
}

eastl::vector<DirectoryEntry> FileSystem::listDirectoryRecursive(
    const fs::path& path, const fs::path& relative_root,
    int32_t max_depth) const {
  eastl::vector<DirectoryEntry> entries;
  listDirectoryRecursiveImpl(path, relative_root, max_depth, entries);
  return entries;
}

bool FileSystem::readBinary(const fs::path& path,
                            eastl::vector<uint8_t>& out_bytes) const {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream.is_open()) {
    LOG_ERROR("[FileSystem] readBinary failed to open: {} (errno={})",
              path.generic_string(), std::strerror(errno));
    return false;
  }

  const std::streamsize size = stream.tellg();
  if (size < 0) {
    LOG_ERROR("[FileSystem] readBinary tellg failed: {}",
              path.generic_string());
    return false;
  }

  stream.seekg(0, std::ios::beg);
  out_bytes.resize(static_cast<size_t>(size));
  if (size > 0 && !stream.read(reinterpret_cast<char*>(out_bytes.data()),
                               static_cast<std::streamsize>(size))) {
    LOG_ERROR("[FileSystem] readBinary read failed: {}",
              path.generic_string());
    return false;
  }
  return true;
}

bool FileSystem::readText(const fs::path& path, eastl::string& out_text) const {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream.is_open()) {
    LOG_ERROR("[FileSystem] readText failed to open: {} (errno={})",
              path.generic_string(), std::strerror(errno));
    return false;
  }

  const std::streamsize size = stream.tellg();
  if (size < 0) {
    LOG_ERROR("[FileSystem] readText tellg failed: {}",
              path.generic_string());
    return false;
  }
  stream.seekg(0, std::ios::beg);

  out_text.resize(static_cast<size_t>(size));
  if (size > 0 &&
      !stream.read(out_text.data(), static_cast<std::streamsize>(size))) {
    LOG_ERROR("[FileSystem] readText read failed: {}", path.generic_string());
    return false;
  }
  return true;
}

bool FileSystem::writeBinary(const fs::path& path, const void* data,
                             size_t size) const {
  std::error_code ec;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
      LOG_ERROR("[FileSystem] writeBinary create_directories failed: {} ({})",
                path.parent_path().generic_string(), ec.message());
      return false;
    }
  }

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) {
    LOG_ERROR("[FileSystem] writeBinary failed to open: {} (errno={})",
              path.generic_string(), std::strerror(errno));
    return false;
  }
  if (size > 0 && !stream.write(static_cast<const char*>(data),
                                static_cast<std::streamsize>(size))) {
    LOG_ERROR("[FileSystem] writeBinary write failed: {}",
              path.generic_string());
    return false;
  }
  return true;
}

bool FileSystem::writeText(const fs::path& path,
                           const eastl::string& text) const {
  return writeBinary(path, text.data(), text.size());
}

bool FileSystem::ensureParentDirectory(const fs::path& path) const {
  if (!path.has_parent_path()) {
    return true;
  }
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    LOG_ERROR("[FileSystem] ensureParentDirectory failed: {} ({})",
              path.parent_path().generic_string(), ec.message());
    return false;
  }
  return true;
}

bool FileSystem::copyFile(const fs::path& src, const fs::path& dst,
                          bool overwrite) const {
  std::error_code ec;
  if (!fs::is_regular_file(src, ec)) {
    LOG_ERROR("[FileSystem] copyFile source is not a file: {}",
              src.generic_string());
    return false;
  }
  if (!overwrite && fs::exists(dst, ec)) {
    LOG_WARN("[FileSystem] copyFile destination exists: {}",
             dst.generic_string());
    return false;
  }
  if (!ensureParentDirectory(dst)) {
    return false;
  }
  fs::copy_file(src, dst,
                overwrite ? fs::copy_options::overwrite_existing
                          : fs::copy_options::none,
                ec);
  if (ec) {
    LOG_ERROR("[FileSystem] copyFile failed {} -> {} ({})",
              src.generic_string(), dst.generic_string(), ec.message());
    return false;
  }
  return true;
}

bool FileSystem::movePath(const fs::path& src, const fs::path& dst) const {
  std::error_code ec;
  if (!fs::exists(src, ec)) {
    LOG_ERROR("[FileSystem] movePath source missing: {}", src.generic_string());
    return false;
  }
  if (!ensureParentDirectory(dst)) {
    return false;
  }
  fs::rename(src, dst, ec);
  if (!ec) {
    return true;
  }
  if (!copyFile(src, dst, false)) {
    return false;
  }
  fs::remove(src, ec);
  if (ec) {
    LOG_ERROR("[FileSystem] movePath remove after copy failed: {} ({})",
              src.generic_string(), ec.message());
    return false;
  }
  return true;
}

}  // namespace Blunder
