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

  m_asset_root = m_project_root / info.asset_subdir;
  m_shader_root = m_project_root / info.shader_subdir;

  m_is_initialized = true;

  LOG_INFO("[FileSystem] executable dir: {}",
           m_executable_dir.generic_string());
  LOG_INFO("[FileSystem] project root  : {}", m_project_root.generic_string());
  LOG_INFO("[FileSystem] asset root    : {}", m_asset_root.generic_string());
  LOG_INFO("[FileSystem] shader root   : {}", m_shader_root.generic_string());
}

void FileSystem::shutdown() {
  m_is_initialized = false;
  m_project_root.clear();
  m_asset_root.clear();
  m_shader_root.clear();
  m_executable_dir.clear();
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

}  // namespace Blunder
