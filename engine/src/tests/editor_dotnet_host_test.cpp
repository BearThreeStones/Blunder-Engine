// Editor-style single ObjectDB proof: process-linked ObjectDB + DotNetHost
// with blunder_native_abi_fill_from_process. Object create and Tick stay in
// this process (engine_runtime + blunder_engine_c_static). Do NOT LoadLibrary
// SHARED blunder_engine_c for Object traffic.

#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/script/dotnet_host.h"

#include <cstdio>
#include <filesystem>

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

std::filesystem::path gameAssemblyPath() {
#if defined(BLUNDER_DOTNET_HOST_GAME_DLL)
  return std::filesystem::path(BLUNDER_DOTNET_HOST_GAME_DLL);
#else
  return scriptHostDir() / "DotnetHostGame.dll";
#endif
}

}  // namespace

int main() {
  using namespace Blunder;

  ObjectDB::clear();
  ClassDB::initialize();

  const std::filesystem::path host_dir = scriptHostDir();
  const std::filesystem::path dll = host_dir / "Blunder.ScriptHost.dll";
  const std::filesystem::path runtimeconfig =
      host_dir / "Blunder.ScriptHost.runtimeconfig.json";
  const std::filesystem::path game_dll = gameAssemblyPath();

  // Process ObjectDB (editor path) — not SHARED LoadLibrary.
  const ObjectId oid = ObjectDB::create();
  expect_true("create object via ObjectDB", isValid(oid));
  Object* object = ObjectDB::get(oid);
  expect_true("object pointer", object != nullptr);

  BlunderNativeAbi native_abi{};
  blunder_native_abi_fill_from_process(&native_abi);
  expect_true("process abi object_create",
              native_abi.object_create != nullptr);
  expect_true("process abi lifecycle_set_tick_hook",
              native_abi.lifecycle_set_tick_hook != nullptr);

  DotNetHost host;
  eastl::string err;
  const bool started = host.start(dll, runtimeconfig, native_abi, err);
  if (!started) {
    std::fprintf(stderr, "start error: %s\n", err.c_str());
    std::fprintf(stderr, "expected ScriptHost at: %s\n",
                 dll.string().c_str());
  }
  expect_true("start succeeds", started);
  expect_true("isRunning after start", host.isRunning());

  if (started && object != nullptr) {
    const bool loaded = host.loadGameAssembly(game_dll, err);
    if (!loaded) {
      std::fprintf(stderr, "loadGameAssembly error: %s\n", err.c_str());
      std::fprintf(stderr, "expected game dll at: %s\n",
                   game_dll.string().c_str());
    }
    expect_true("load game", loaded);

    BehaviourId bid{};
    const bool attached = host.attachBehaviour(
        oid, "DotnetHostGame.ProbeBehaviour", &bid, err);
    if (!attached) {
      std::fprintf(stderr, "attachBehaviour error: %s\n", err.c_str());
    }
    expect_true("attach", attached);
    expect_true("behaviour id", static_cast<uint64_t>(bid) != 0);

    // Editor frame path: process LifecycleDispatch on process Object.
    LifecycleDispatch::invokeReady(object);
    LifecycleDispatch::invokeTick(object, 0.016f);

    const bool resolved = host.resolveProbeTickCount(dll, err);
    if (!resolved) {
      std::fprintf(stderr, "resolveProbeTickCount error: %s\n", err.c_str());
    }
    expect_true("resolve probe tick count", resolved);
    const int ticks = host.getProbeTickCount();
    if (ticks != 1) {
      std::fprintf(stderr, "getProbeTickCount=%d (expected 1)\n", ticks);
    }
    expect_true("managed tick on process ObjectDB", ticks == 1);
  }

  ObjectDB::destroy(oid);
  host.shutdown();
  expect_true("not running after shutdown", !host.isRunning());

  ClassDB::shutdown();
  ObjectDB::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "editor_dotnet_host_test OK\n");
  return 0;
}
