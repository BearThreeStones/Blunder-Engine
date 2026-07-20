#include "runtime/function/script/dotnet_host.h"

#include <cstdio>
#include <filesystem>
#include <string>

#include "EASTL/string.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

std::filesystem::path scriptHostDir() {
#if defined(BLUNDER_SCRIPT_HOST_DIR)
  return std::filesystem::path(BLUNDER_SCRIPT_HOST_DIR);
#else
  return std::filesystem::path();
#endif
}

}  // namespace

int main() {
  using namespace Blunder;

  const std::filesystem::path host_dir = scriptHostDir();
  const std::filesystem::path dll = host_dir / "Blunder.ScriptHost.dll";
  const std::filesystem::path runtimeconfig =
      host_dir / "Blunder.ScriptHost.runtimeconfig.json";

  // Non-fatal failure path: missing files must not abort the process.
  {
    DotNetHost missing;
    eastl::string err;
    const bool ok = missing.start("Z:/definitely/missing/Blunder.ScriptHost.dll",
                                  "Z:/definitely/missing/missing.runtimeconfig.json",
                                  err);
    expect_true("missing start returns false", !ok);
    expect_true("missing start sets error", !err.empty());
    expect_true("missing host not running", !missing.isRunning());
  }

  DotNetHost host;
  eastl::string err;
  const bool started = host.start(dll, runtimeconfig, err);
  if (!started) {
    std::fprintf(stderr, "start error: %s\n", err.c_str());
    std::fprintf(stderr, "expected ScriptHost at: %s\n",
                 dll.string().c_str());
  }
  expect_true("start succeeds", started);
  expect_true("isRunning after start", host.isRunning());

  host.shutdown();
  expect_true("not running after shutdown", !host.isRunning());

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "dotnet_host_test OK\n");
  return 0;
}
