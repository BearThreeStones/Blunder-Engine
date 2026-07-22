#include "runtime/project/editor_launch.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

std::vector<char*> makeArgv(std::vector<std::string>& storage) {
  std::vector<char*> argv;
  argv.reserve(storage.size());
  for (std::string& s : storage) {
    argv.push_back(s.data());
  }
  return argv;
}

}  // namespace

int main() {
  using namespace Blunder;
  namespace fs = std::filesystem;

  {
    std::vector<std::string> args = {"engine_editor"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("release no-args fails", !opts.ok);
    expect_true("error mentions project_manager",
                opts.error.find("project_manager") != eastl::string::npos);
  }

  {
    std::vector<std::string> args = {"engine_editor", "--project-root",
                                     "C:/Games/Demo"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("explicit root ok", opts.ok);
    expect_true("root path set", opts.project_root == fs::path("C:/Games/Demo"));
  }

  {
    std::vector<std::string> args = {"engine_editor"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), true,
        fs::path("E:/Dev/Blunder-Engine"));
    expect_true("debug compiled root ok", opts.ok);
    expect_true("debug root used",
                opts.project_root == fs::path("E:/Dev/Blunder-Engine"));
  }

  {
    std::vector<std::string> args = {"engine_editor", "--project-root",
                                     "C:/Games/Demo"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), true,
        fs::path("E:/Dev/Blunder-Engine"));
    expect_true("cli root wins over debug", opts.ok);
    expect_true("cli root used",
                opts.project_root == fs::path("C:/Games/Demo"));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
