#include "runtime/project/project_relaunch.h"

#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <spawn.h>
extern char** environ;
#endif

namespace Blunder {
namespace {

std::filesystem::path queryExecutablePath() {
#ifdef _WIN32
  wchar_t buffer[MAX_PATH];
  const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return {};
  }
  return std::filesystem::path(buffer);
#elif defined(__linux__)
  char buffer[PATH_MAX];
  const ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (len <= 0) {
    return {};
  }
  buffer[len] = '\0';
  return std::filesystem::path(buffer);
#elif defined(__APPLE__)
  char buffer[PATH_MAX];
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    return {};
  }
  return std::filesystem::path(buffer);
#else
  return {};
#endif
}

#ifdef _WIN32
std::wstring quoteWindowsArg(const std::wstring& arg) {
  if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
    return arg;
  }
  std::wstring out = L"\"";
  for (wchar_t c : arg) {
    if (c == L'"') {
      out += L"\\\"";
    } else {
      out += c;
    }
  }
  out += L'"';
  return out;
}
#endif

std::filesystem::path editorExecutableName() {
#ifdef _WIN32
  return "engine_editor.exe";
#else
  return "engine_editor";
#endif
}

}  // namespace

eastl::vector<eastl::string> buildProjectOpenArgv(
    const std::filesystem::path& project_root) {
  eastl::vector<eastl::string> args;
  args.push_back("engine_editor");
  args.push_back("--project-root");
  args.push_back(project_root.generic_string().c_str());
  return args;
}

std::filesystem::path resolveEditorExecutablePath() {
  const std::filesystem::path self = queryExecutablePath();
  if (self.empty()) {
    return {};
  }
  return self.parent_path() / editorExecutableName();
}

bool relaunchEditorWithProject(const std::filesystem::path& project_root) {
  if (project_root.empty()) {
    return false;
  }

  const std::filesystem::path exe = resolveEditorExecutablePath();
  if (exe.empty()) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::is_regular_file(exe, ec) || ec) {
    // Sibling engine_editor.exe missing — usually output dirs not co-located.
    return false;
  }

#ifdef _WIN32
  eastl::vector<eastl::string> storage = buildProjectOpenArgv(project_root);
  storage[0] = exe.string().c_str();

  std::wstring command_line;
  for (size_t i = 0; i < storage.size(); ++i) {
    if (i > 0) {
      command_line.push_back(L' ');
    }
    command_line += quoteWindowsArg(std::filesystem::path(storage[i].c_str()).wstring());
  }

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::wstring mutable_cmd = command_line;
  const BOOL ok = CreateProcessW(exe.c_str(), mutable_cmd.data(), nullptr,
                                 nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
  if (!ok) {
    return false;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
#elif defined(__linux__) || defined(__APPLE__)
  eastl::vector<eastl::string> storage = buildProjectOpenArgv(project_root);
  storage[0] = exe.string().c_str();

  std::vector<char*> argv;
  argv.reserve(storage.size() + 1);
  for (eastl::string& arg : storage) {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);

  pid_t pid = 0;
  const int rc = posix_spawn(&pid, exe.string().c_str(), nullptr, nullptr,
                             argv.data(), environ);
  return rc == 0;
#else
  (void)exe;
  return false;
#endif
}

}  // namespace Blunder
