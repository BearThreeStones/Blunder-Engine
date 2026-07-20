// Approach A (dual-ObjectDB): Object create + lifecycle invoke go only through
// ONE in-process image of SHARED blunder_engine_c — the copy staged beside
// Blunder.ScriptHost (bin/<Config>/). The test must not link the import lib
// against a second copy next to the exe (that yields two ObjectDBs).
// Managed Native uses the registered SHARED exports (not process static).

#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/function/script/dotnet_host.h"

#include <cstdio>
#include <filesystem>
#include <string>

#include "EASTL/string.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

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

using AbiVersionFn = int (*)();
using ObjectCreateFn = uint64_t (*)();
using ObjectDestroyFn = int (*)(uint64_t);
using ObjectIsValidFn = int (*)(uint64_t);
using LifecycleInvokeReadyFn = int (*)(uint64_t);
using LifecycleInvokeTickFn = int (*)(uint64_t, float);
using FillFromModuleFn = int (*)(BlunderNativeAbi*, void*);

struct EngineCAbi {
#ifdef _WIN32
  HMODULE lib{nullptr};
#else
  void* lib{nullptr};
#endif
  AbiVersionFn abi_version{nullptr};
  ObjectCreateFn object_create{nullptr};
  ObjectDestroyFn object_destroy{nullptr};
  ObjectIsValidFn object_is_valid{nullptr};
  LifecycleInvokeReadyFn invoke_ready{nullptr};
  LifecycleInvokeTickFn invoke_tick{nullptr};
  FillFromModuleFn fill_from_module{nullptr};

  bool load(const std::filesystem::path& dll_path, eastl::string& out_error) {
    out_error.clear();
#ifdef _WIN32
    lib = ::LoadLibraryW(dll_path.wstring().c_str());
    if (lib == nullptr) {
      out_error = "LoadLibrary failed for blunder_engine_c";
      return false;
    }
    auto sym = [this](const char* name) -> void* {
      return reinterpret_cast<void*>(::GetProcAddress(lib, name));
    };
#else
    lib = ::dlopen(dll_path.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (lib == nullptr) {
      out_error = "dlopen failed for blunder_engine_c";
      return false;
    }
    auto sym = [this](const char* name) -> void* { return ::dlsym(lib, name); };
#endif
    abi_version = reinterpret_cast<AbiVersionFn>(sym("blunder_engine_abi_version"));
    object_create = reinterpret_cast<ObjectCreateFn>(sym("blunder_object_create"));
    object_destroy = reinterpret_cast<ObjectDestroyFn>(sym("blunder_object_destroy"));
    object_is_valid = reinterpret_cast<ObjectIsValidFn>(sym("blunder_object_is_valid"));
    invoke_ready =
        reinterpret_cast<LifecycleInvokeReadyFn>(sym("blunder_lifecycle_invoke_ready"));
    invoke_tick =
        reinterpret_cast<LifecycleInvokeTickFn>(sym("blunder_lifecycle_invoke_tick"));
    fill_from_module =
        reinterpret_cast<FillFromModuleFn>(sym("blunder_native_abi_fill_from_module"));
    if (abi_version == nullptr || object_create == nullptr ||
        object_destroy == nullptr || object_is_valid == nullptr ||
        invoke_ready == nullptr || invoke_tick == nullptr ||
        fill_from_module == nullptr) {
      out_error =
          "blunder_engine_c exports missing (need invoke_ready/tick + "
          "fill_from_module)";
      return false;
    }
    return true;
  }
};

}  // namespace

int main() {
  using namespace Blunder;

  const std::filesystem::path host_dir = scriptHostDir();
  const std::filesystem::path dll = host_dir / "Blunder.ScriptHost.dll";
  const std::filesystem::path runtimeconfig =
      host_dir / "Blunder.ScriptHost.runtimeconfig.json";
  const std::filesystem::path game_dll = gameAssemblyPath();
  const std::filesystem::path engine_c_dll = host_dir / "blunder_engine_c.dll";

  // Non-fatal failure path: missing files must not abort the process.
  {
    DotNetHost missing;
    eastl::string err;
    const BlunderNativeAbi empty_abi{};
    const bool ok = missing.start(
        "Z:/definitely/missing/Blunder.ScriptHost.dll",
        "Z:/definitely/missing/missing.runtimeconfig.json", empty_abi, err);
    expect_true("missing start returns false", !ok);
    expect_true("missing start sets error", !err.empty());
    expect_true("missing host not running", !missing.isRunning());
  }

  EngineCAbi abi;
  eastl::string abi_err;
  const bool abi_loaded = abi.load(engine_c_dll, abi_err);
  if (!abi_loaded) {
    std::fprintf(stderr, "engine_c load error: %s\n", abi_err.c_str());
    std::fprintf(stderr, "expected at: %s\n", engine_c_dll.string().c_str());
  }
  expect_true("load shared blunder_engine_c", abi_loaded);
  if (!abi_loaded) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }

  expect_true("abi version via shared dll", abi.abi_version() == 2);

  BlunderNativeAbi native_abi{};
  const int fill_rc = abi.fill_from_module(&native_abi, abi.lib);
  expect_true("fill native abi from shared module",
              fill_rc == BLUNDER_ENGINE_OK);
  if (fill_rc != BLUNDER_ENGINE_OK) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }

  // Object lives in the ScriptHost-staged SHARED ObjectDB (Approach A).
  const uint64_t oid = abi.object_create();
  expect_true("create object via c-abi", oid != 0);
  expect_true("object valid via c-abi", abi.object_is_valid(oid) == 1);

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

  if (started) {
    const bool loaded = host.loadGameAssembly(game_dll, err);
    if (!loaded) {
      std::fprintf(stderr, "loadGameAssembly error: %s\n", err.c_str());
      std::fprintf(stderr, "expected game dll at: %s\n",
                   game_dll.string().c_str());
    }
    expect_true("load game", loaded);

    BehaviourId bid{};
    const bool attached = host.attachBehaviour(
        static_cast<ObjectId>(oid), "DotnetHostGame.ProbeBehaviour", &bid, err);
    if (!attached) {
      std::fprintf(stderr, "attachBehaviour error: %s\n", err.c_str());
    }
    expect_true("attach", attached);
    expect_true("behaviour id", static_cast<uint64_t>(bid) != 0);

    BehaviourId sibling_bid{};
    const bool sibling_attached = host.attachBehaviour(
        static_cast<ObjectId>(oid), "DotnetHostGame.SiblingProbeBehaviour",
        &sibling_bid, err);
    if (!sibling_attached) {
      std::fprintf(stderr, "attach sibling error: %s\n", err.c_str());
    }
    expect_true("attach sibling", sibling_attached);
    expect_true("sibling behaviour id", static_cast<uint64_t>(sibling_bid) != 0);
    expect_true("distinct behaviour ids",
                static_cast<uint64_t>(bid) != static_cast<uint64_t>(sibling_bid));

    // LifecycleDispatch must run inside the same SHARED DLL ObjectDB.
    expect_true("invoke ready via c-abi", abi.invoke_ready(oid) == 0);
    expect_true("invoke tick via c-abi", abi.invoke_tick(oid, 0.016f) == 0);

    const bool resolved = host.resolveProbeTickCount(dll, err);
    if (!resolved) {
      std::fprintf(stderr, "resolveProbeTickCount error: %s\n", err.c_str());
    }
    expect_true("resolve probe tick count", resolved);
    const int ticks = host.getProbeTickCount();
    if (ticks != 1) {
      std::fprintf(stderr, "getProbeTickCount=%d (expected 1)\n", ticks);
    }
    expect_true("managed tick", ticks == 1);

    const int sibling_found = host.getProbeSiblingFound();
    if (sibling_found != 1) {
      std::fprintf(stderr, "getProbeSiblingFound=%d (expected 1)\n",
                   sibling_found);
    }
    expect_true("GetBehaviour finds sibling after AttachBehaviour",
                sibling_found == 1);
  }

  expect_true("destroy object via c-abi", abi.object_destroy(oid) == 0);

  host.shutdown();
  expect_true("not running after shutdown", !host.isRunning());

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "dotnet_host_test OK\n");
  return 0;
}
