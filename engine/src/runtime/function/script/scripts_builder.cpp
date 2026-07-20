#include "runtime/function/script/scripts_builder.h"

#include "runtime/project/project_file.h"

#include <cstdlib>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Blunder {

namespace {

namespace fs = std::filesystem;

fs::path queryExecutableDir() {
#ifdef _WIN32
  wchar_t buffer[MAX_PATH];
  const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return {};
  }
  return fs::path(buffer).parent_path();
#else
  std::error_code ec;
  return fs::current_path(ec);
#endif
}

fs::path findScriptsCsproj(const fs::path& project_root) {
  const fs::path scripts = project_root / "Scripts";
  std::error_code ec;
  if (!fs::is_directory(scripts, ec)) {
    return {};
  }
  for (const fs::directory_entry& entry : fs::directory_iterator(scripts, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    if (entry.path().extension() == ".csproj") {
      return entry.path();
    }
  }
  return {};
}

fs::path findBuiltDll(const fs::path& out_dir, const fs::path& csproj) {
  const fs::path expected = out_dir / (csproj.stem().string() + ".dll");
  std::error_code ec;
  if (fs::is_regular_file(expected, ec)) {
    return expected;
  }
  if (!fs::is_directory(out_dir, ec)) {
    return {};
  }
  for (const fs::directory_entry& entry : fs::directory_iterator(out_dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    if (entry.path().extension() != ".dll") {
      continue;
    }
    const std::string name = entry.path().filename().string();
    if (name == "Blunder.Api.dll" || name == "Blunder.ScriptHost.dll") {
      continue;
    }
    return entry.path();
  }
  return {};
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

bool runDotnetBuild(const fs::path& csproj, const fs::path& out_dir,
                    const fs::path& engine_dir, eastl::string& out_error) {
  std::wstring command_line = L"dotnet build ";
  command_line += quoteWindowsArg(csproj.wstring());
  command_line += L" -c Debug -o ";
  command_line += quoteWindowsArg(out_dir.wstring());
  if (!engine_dir.empty()) {
    command_line += L" -p:BlunderEngineDir=";
    // Trailing separator so $(BlunderEngineDir)Blunder.Api.dll concatenates.
    fs::path dir = engine_dir;
    std::wstring dir_str = dir.wstring();
    if (!dir_str.empty() && dir_str.back() != L'\\' && dir_str.back() != L'/') {
      dir_str.push_back(L'\\');
    }
    command_line += quoteWindowsArg(dir_str);
  }

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::wstring mutable_cmd = command_line;
  const BOOL ok =
      CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE,
                     CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  if (!ok) {
    out_error = "failed to start dotnet build";
    return false;
  }
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(pi.hProcess, &exit_code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  if (exit_code != 0) {
    out_error = "dotnet build failed";
    return false;
  }
  return true;
}
#else
bool runDotnetBuild(const fs::path& csproj, const fs::path& out_dir,
                    const fs::path& engine_dir, eastl::string& out_error) {
  eastl::string command = "dotnet build \"";
  command += csproj.string().c_str();
  command += "\" -c Debug -o \"";
  command += out_dir.string().c_str();
  command += "\"";
  if (!engine_dir.empty()) {
    command += " -p:BlunderEngineDir=\"";
    command += engine_dir.string().c_str();
    if (!command.empty() && command.back() != '/') {
      command += "/";
    }
    command += "\"";
  }
  const int rc = std::system(command.c_str());
  if (rc != 0) {
    out_error = "dotnet build failed";
    return false;
  }
  return true;
}
#endif

}  // namespace

ScriptsBuildResult buildProjectScripts(const fs::path& project_root) {
  ScriptsBuildResult result;
  if (project_root.empty()) {
    result.error = "project root is required";
    return result;
  }
  if (!isProjectDirectory(project_root)) {
    result.error = "not a Blunder project";
    return result;
  }

  const fs::path csproj = findScriptsCsproj(project_root);
  if (csproj.empty()) {
    result.error = "Scripts/*.csproj not found";
    return result;
  }

  const fs::path out_dir = project_root / ".blunder" / "scripts_bin";
  std::error_code ec;
  fs::create_directories(out_dir, ec);
  if (ec) {
    result.error = "failed to create scripts_bin";
    return result;
  }

  const fs::path engine_dir = queryExecutableDir();
  if (!runDotnetBuild(csproj, out_dir, engine_dir, result.error)) {
    return result;
  }

  result.output_dll = findBuiltDll(out_dir, csproj);
  if (result.output_dll.empty()) {
    result.error = "dotnet build succeeded but output DLL missing";
    return result;
  }
  result.ok = true;
  result.error.clear();
  return result;
}

}  // namespace Blunder
