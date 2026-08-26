// Lifecycle exception isolation: ThrowingTickBehaviour + ProbeBehaviour;
// ThrowingMessageBehaviour + MessageProbeBehaviour.

#include "runtime/core/log/console_ring.h"
#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/core/reflection/lifecycle.h"
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
  ConsoleRing::instance().clear();

  const ObjectId object = ObjectDB::create();
  expect_true("create object", isValid(object));

  BlunderNativeAbi native_abi{};
  blunder_native_abi_fill_from_process(&native_abi);

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
  }
  expect_true("start succeeds", started);

  if (started) {
    expect_true("load game", host.loadGameAssembly(game_dll, err));

    BehaviourId throw_bid{};
    BehaviourId probe_bid{};
    expect_true("attach throwing tick",
                host.attachBehaviour(object,
                                     "DotnetHostGame.ThrowingTickBehaviour",
                                     &throw_bid, err));
    expect_true(
        "attach probe",
        host.attachBehaviour(object, "DotnetHostGame.ProbeBehaviour", &probe_bid,
                             err));
    expect_true("resolve probes", host.resolveProbeTickCount(dll, err));

    Object* live = ObjectDB::get(object);
    expect_true("object live", live != nullptr);
    if (live != nullptr) {
      LifecycleDispatch::invokeTick(live, 0.016f);
    }

    const int ticks = host.getProbeTickCount();
    expect_true("sibling still ticked", ticks >= 1);

    bool saw_error = false;
    for (const ConsoleMessage& msg : ConsoleRing::instance().snapshot()) {
      if (msg.severity == ConsoleSeverity::Error &&
          msg.text.find("throwing tick") != std::string::npos) {
        saw_error = true;
        expect_true("tick error has stack", !msg.stack.empty());
      }
    }
    expect_true("tick throw logged error", saw_error);

    // Message fan-out: first throws, second still receives.
    ConsoleRing::instance().clear();
    ObjectDB::destroy(object);
    const ObjectId msg_object = ObjectDB::create();
    BehaviourId throw_msg{};
    BehaviourId probe_msg{};
    expect_true(
        "attach throwing message",
        host.attachBehaviour(msg_object,
                             "DotnetHostGame.ThrowingMessageBehaviour",
                             &throw_msg, err));
    expect_true(
        "attach message probe",
        host.attachBehaviour(msg_object,
                             "DotnetHostGame.MessageProbeBehaviour", &probe_msg,
                             err));

    BlunderMessageId ping_id = 0;
    expect_true("register Ping",
                blunder_message_register("Ping", &ping_id) == BLUNDER_ENGINE_OK);
    expect_true(
        "send Ping",
        blunder_message_send(static_cast<BlunderObjectId>(msg_object), ping_id,
                             nullptr, 0) == BLUNDER_ENGINE_OK);

    expect_true("message probe fired", host.getMessageProbeCount() == 1);

    bool saw_msg_error = false;
    for (const ConsoleMessage& msg : ConsoleRing::instance().snapshot()) {
      if (msg.severity == ConsoleSeverity::Error &&
          msg.text.find("throwing message") != std::string::npos) {
        saw_msg_error = true;
      }
    }
    expect_true("message throw logged", saw_msg_error);

    ObjectDB::destroy(msg_object);
  }

  host.shutdown();
  ClassDB::shutdown();
  ObjectDB::clear();
  MessageDispatch::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "lifecycle_exception_dotnet_test OK\n");
  return 0;
}
