// Cross-Object Message e2e: process-linked ObjectDB + DotNetHost with
// blunder_native_abi_fill_from_process. Attach MessageProbeBehaviour, register
// "Ping", send via C-ABI blunder_message_send, assert OnMessage fired.

#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/core/reflection/message_dispatch.h"
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
  MessageDispatch::clear();
  ClassDB::initialize();

  const ObjectId target = ObjectDB::create();
  const ObjectId other = ObjectDB::create();
  expect_true("create target object", isValid(target));
  expect_true("create other object", isValid(other));

  BlunderNativeAbi native_abi{};
  blunder_native_abi_fill_from_process(&native_abi);
  expect_true("process abi message_register",
              native_abi.message_register != nullptr);
  expect_true("process abi message_send", native_abi.message_send != nullptr);

  const std::filesystem::path host_dir = scriptHostDir();
  const std::filesystem::path dll = host_dir / "Blunder.ScriptHost.dll";
  const std::filesystem::path runtimeconfig =
      host_dir / "Blunder.ScriptHost.runtimeconfig.json";
  const std::filesystem::path game_dll = gameAssemblyPath();

  DotNetHost host;
  eastl::string err;
  const bool started = host.start(dll, runtimeconfig, native_abi, err);
  if (!started) {
    std::fprintf(stderr, "start error: %s\n", err.c_str());
    std::fprintf(stderr, "expected ScriptHost at: %s\n", dll.string().c_str());
  }
  expect_true("start succeeds", started);

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
        target, "DotnetHostGame.MessageProbeBehaviour", &bid, err);
    if (!attached) {
      std::fprintf(stderr, "attachBehaviour error: %s\n", err.c_str());
    }
    expect_true("attach MessageProbe", attached);
    expect_true("behaviour id", static_cast<uint64_t>(bid) != 0);

    BlunderMessageId ping_id = 0;
    const int reg_rc = blunder_message_register("Ping", &ping_id);
    expect_true("register Ping", reg_rc == BLUNDER_ENGINE_OK && ping_id != 0);

    const int send_rc =
        blunder_message_send(static_cast<BlunderObjectId>(target), ping_id,
                             nullptr, 0);
    expect_true("send Ping", send_rc == BLUNDER_ENGINE_OK);

    const bool resolved = host.resolveProbeTickCount(dll, err);
    if (!resolved) {
      std::fprintf(stderr, "resolveProbeTickCount error: %s\n", err.c_str());
    }
    expect_true("resolve message probe exports", resolved);

    const int count = host.getMessageProbeCount();
    if (count != 1) {
      std::fprintf(stderr, "getMessageProbeCount=%d (expected 1)\n", count);
    }
    expect_true("OnMessage fired once", count == 1);

    const int last_id = host.getMessageProbeLastId();
    if (static_cast<uint32_t>(last_id) != ping_id) {
      std::fprintf(stderr, "getMessageProbeLastId=%d (expected %u)\n", last_id,
                   ping_id);
    }
    expect_true("LastId matches Ping", static_cast<uint32_t>(last_id) == ping_id);

    // Cross-Object: send to a different Object does not invoke target probe.
    const int send_other_rc =
        blunder_message_send(static_cast<BlunderObjectId>(other), ping_id,
                             nullptr, 0);
    expect_true("send Ping to other", send_other_rc == BLUNDER_ENGINE_OK);
    const int count_after_other = host.getMessageProbeCount();
    if (count_after_other != 1) {
      std::fprintf(stderr,
                   "getMessageProbeCount after other send=%d (expected 1)\n",
                   count_after_other);
    }
    expect_true("OnMessage not fired on other object", count_after_other == 1);
  }

  ObjectDB::destroy(other);
  ObjectDB::destroy(target);
  host.shutdown();
  expect_true("not running after shutdown", !host.isRunning());

  ClassDB::shutdown();
  ObjectDB::clear();
  MessageDispatch::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "object_message_dotnet_test OK\n");
  return 0;
}
