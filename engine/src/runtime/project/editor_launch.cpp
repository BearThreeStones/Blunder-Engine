#include "runtime/project/editor_launch.h"
#include "runtime/project/project_root_cli.h"

#include <cstdlib>
#include <cstring>
#include <utility>

namespace Blunder {

namespace {

bool eq(const char* a, const char* b) {
  return a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
}

bool takeValue(int argc, char** argv, int& i, eastl::string& out) {
  if (i + 1 >= argc || argv[i + 1] == nullptr) {
    return false;
  }
  ++i;
  out = argv[i];
  return true;
}

float parseFloat(const eastl::string& text, float fallback) {
  if (text.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const float v = std::strtof(text.c_str(), &end);
  if (end == text.c_str()) {
    return fallback;
  }
  return v;
}

uint32_t parseU32(const eastl::string& text) {
  if (text.empty()) {
    return 0;
  }
  char* end = nullptr;
  const unsigned long v = std::strtoul(text.c_str(), &end, 10);
  if (end == text.c_str()) {
    return 0;
  }
  return static_cast<uint32_t>(v);
}

EditorSessionLaunch failLaunch(const char* code, const char* message) {
  EditorSessionLaunch options;
  options.ok = false;
  options.failure_code = code;
  options.error = message;
  return options;
}

}  // namespace

bool isMachineCliVerb(const char* arg) {
  return eq(arg, "query") || eq(arg, "op") || eq(arg, "diagnose") ||
         eq(arg, "capture") || eq(arg, "play-frame") || eq(arg, "save");
}

EditorSessionLaunch resolveEditorSessionLaunch(
    int argc, char** argv, bool debug_build,
    const std::filesystem::path& compiled_project_root) {
  std::filesystem::path cli_root;
  bool headless = false;
  bool windowed = false;
  bool mcp = false;
  eastl::string scene;
  MachineCliArgs cli;

  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (arg == nullptr) {
      continue;
    }
    if (eq(arg, "--project-root")) {
      takeSpacedExistingPath(argc, argv, i, cli_root, isMachineCliVerb);
      continue;
    }
    if (eq(arg, "--headless")) {
      headless = true;
      continue;
    }
    if (eq(arg, "--windowed")) {
      windowed = true;
      continue;
    }
    if (eq(arg, "--mcp")) {
      mcp = true;
      continue;
    }
    if (eq(arg, "--scene")) {
      takeValue(argc, argv, i, scene);
      continue;
    }
    if (eq(arg, "--subject")) {
      takeValue(argc, argv, i, cli.subject);
      continue;
    }
    if (eq(arg, "--out")) {
      takeValue(argc, argv, i, cli.out_path);
      continue;
    }
    if (eq(arg, "--save")) {
      cli.save = true;
      continue;
    }
    if (eq(arg, "--steps")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.steps = parseU32(value);
      }
      continue;
    }
    if (eq(arg, "--name") || eq(arg, "--entity")) {
      takeValue(argc, argv, i, cli.entity);
      continue;
    }
    if (eq(arg, "--asset")) {
      takeValue(argc, argv, i, cli.asset);
      continue;
    }
    if (eq(arg, "--tx")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.tx = parseFloat(value, 0.0f);
      }
      continue;
    }
    if (eq(arg, "--ty")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.ty = parseFloat(value, 0.0f);
      }
      continue;
    }
    if (eq(arg, "--tz")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.tz = parseFloat(value, 0.0f);
      }
      continue;
    }
    if (eq(arg, "--qx")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.qx = parseFloat(value, 0.0f);
      }
      continue;
    }
    if (eq(arg, "--qy")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.qy = parseFloat(value, 0.0f);
      }
      continue;
    }
    if (eq(arg, "--qz")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.qz = parseFloat(value, 0.0f);
      }
      continue;
    }
    if (eq(arg, "--qw")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.qw = parseFloat(value, 1.0f);
      }
      continue;
    }
    if (eq(arg, "--sx")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.sx = parseFloat(value, 1.0f);
      }
      continue;
    }
    if (eq(arg, "--sy")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.sy = parseFloat(value, 1.0f);
      }
      continue;
    }
    if (eq(arg, "--sz")) {
      eastl::string value;
      if (takeValue(argc, argv, i, value)) {
        cli.sz = parseFloat(value, 1.0f);
      }
      continue;
    }
    if (arg[0] == '-') {
      continue;
    }
    if (cli.verb.empty() && isMachineCliVerb(arg)) {
      cli.verb = arg;
      continue;
    }
  }

  const bool cli_adapter = !cli.verb.empty();
  if (mcp && cli_adapter) {
    return failLaunch(k_request_launch_adapter_conflict,
                      "Pass --mcp or a CLI verb, not both.");
  }

  const bool adapter = mcp || cli_adapter;
  if (adapter && windowed) {
    return failLaunch(k_request_adapter_windowed_forbidden,
                      "CLI/MCP Editor Session cannot be windowed.");
  }

  if (adapter) {
    headless = true;
  }

  EditorSessionLaunch options;
  if (!cli_root.empty()) {
    options.ok = true;
    options.project_root = cli_root;
  } else if (!adapter && debug_build && !compiled_project_root.empty()) {
    options.ok = true;
    options.project_root = compiled_project_root;
  } else if (adapter) {
    return failLaunch(k_request_launch_project_root_required,
                      "CLI/MCP requires --project-root <path>.");
  } else {
    options.ok = false;
    options.error =
        "No project root. Launch project_manager.exe, or pass "
        "--project-root <path>.";
    return options;
  }

  options.headless = headless;
  options.scene = scene;
  options.cli = std::move(cli);
  if (mcp) {
    options.adapter = MachineAdapterKind::mcp;
  } else if (cli_adapter) {
    options.adapter = MachineAdapterKind::cli;
  }
  return options;
}

}  // namespace Blunder
