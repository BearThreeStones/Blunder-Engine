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
    expect_true("windowed by default", !opts.headless);
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

  {
    std::vector<std::string> args = {"engine_editor", "--project-root",
                                     "C:/Games/Demo", "--headless"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("headless with root ok", opts.ok);
    expect_true("headless flag", opts.headless);
    expect_true("headless root", opts.project_root == fs::path("C:/Games/Demo"));
  }

  {
    std::vector<std::string> args = {"engine_editor", "--headless"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), true,
        fs::path("E:/Dev/Blunder-Engine"));
    expect_true("headless debug compiled root ok", opts.ok);
    expect_true("headless debug flag", opts.headless);
  }

  {
    std::vector<std::string> args = {"engine_editor", "--mcp", "--project-root",
                                     "C:/Games/Demo"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("mcp with root ok", opts.ok);
    expect_true("mcp implies headless", opts.headless);
    expect_true("mcp kind", opts.adapter == MachineAdapterKind::mcp);
    expect_true("mcp scene empty", opts.scene.empty());
  }

  {
    std::vector<std::string> args = {
        "engine_editor", "--project-root", "C:/Games/Demo", "--scene",
        "assets/Scenes/main.scene.asset", "capture", "--subject", "live",
        "--out", "shot.png"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("cli capture ok", opts.ok);
    expect_true("cli implies headless", opts.headless);
    expect_true("cli kind", opts.adapter == MachineAdapterKind::cli);
    expect_true("cli verb", opts.cli.verb == "capture");
    expect_true("cli subject", opts.cli.subject == "live");
    expect_true("cli out", opts.cli.out_path == "shot.png");
    expect_true("cli scene",
                opts.scene == "assets/Scenes/main.scene.asset");
  }

  {
    std::vector<std::string> args = {"engine_editor", "--mcp"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), true,
        fs::path("E:/Dev/Blunder-Engine"));
    expect_true("debug mcp without root fails", !opts.ok);
    expect_true("debug mcp code",
                opts.failure_code == k_request_launch_project_root_required);
  }

  {
    std::vector<std::string> args = {
        "engine_editor", "--mcp", "--project-root", "C:/Games/Demo",
        "--windowed"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("windowed mcp ok", opts.ok);
    expect_true("windowed mcp not headless", !opts.headless);
    expect_true("windowed mcp kind", opts.adapter == MachineAdapterKind::mcp);
  }

  {
    std::vector<std::string> args = {
        "engine_editor", "--project-root", "C:/Games/Demo", "--scene",
        "assets/Scenes/main.scene.asset", "capture", "--subject", "live",
        "--windowed"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("windowed cli fails", !opts.ok);
    expect_true("windowed cli code",
                opts.failure_code == k_request_adapter_windowed_forbidden);
  }

  {
    std::vector<std::string> args = {
        "engine_editor", "--mcp", "--project-root", "C:/Games/Demo", "capture"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("mcp plus verb fails", !opts.ok);
    expect_true("mcp plus verb code",
                opts.failure_code == k_request_launch_adapter_conflict);
  }

  {
    const fs::path tmp =
        fs::temp_directory_path() / "blunder_editor_launch_spaced";
    fs::remove_all(tmp);
    const fs::path prefix = tmp / "Blunder";
    const fs::path full = tmp / "Blunder Projects" / "Test";
    fs::create_directories(prefix);
    fs::create_directories(full);

    std::vector<std::string> args = {"engine_editor", "--project-root",
                                     prefix.generic_string(),
                                     "Projects/Test"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("unquoted spaced root ok", opts.ok);
    expect_true("unquoted spaced root joined",
                fs::equivalent(opts.project_root, full));

    std::vector<std::string> capture_args = {
        "engine_editor", "--project-root", prefix.generic_string(),
        "Projects/Test", "capture", "--subject", "live"};
    auto capture_argv = makeArgv(capture_args);
    const EditorSessionLaunch capture_opts = resolveEditorSessionLaunch(
        static_cast<int>(capture_argv.size()), capture_argv.data(), false,
        fs::path{});
    expect_true("spaced root plus capture ok", capture_opts.ok);
    expect_true("spaced root plus capture joined",
                fs::equivalent(capture_opts.project_root, full));
    expect_true("capture verb not eaten", capture_opts.cli.verb == "capture");

    fs::remove_all(tmp);
  }

  {
    std::vector<std::string> args = {
        "engine_editor", "--project-root", "C:/Games/Demo", "--scene",
        "assets/Scenes/root.scene.asset", "ray", "--ox", "0", "--oy", "0",
        "--oz", "5", "--dz", "-1", "--max-distance", "20", "--mask",
        "4294967295", "--collide-with-areas"};
    auto argv = makeArgv(args);
    const EditorSessionLaunch opts = resolveEditorSessionLaunch(
        static_cast<int>(argv.size()), argv.data(), false, fs::path{});
    expect_true("cli ray ok", opts.ok);
    expect_true("cli ray verb", opts.cli.verb == "ray");
    expect_true("cli ray oz", opts.cli.oz == 5.0f);
    expect_true("cli ray dz", opts.cli.dz == -1.0f);
    expect_true("cli ray max", opts.cli.max_distance == 20.0f);
    expect_true("cli ray mask", opts.cli.mask == 4294967295u);
    expect_true("cli ray areas", opts.cli.collide_with_areas);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
