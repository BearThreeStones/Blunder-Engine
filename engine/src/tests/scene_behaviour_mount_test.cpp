// Editor-style: process ObjectDB + fill_from_process + scene instantiate +
// mountSceneBehaviours → managed ProbeBehaviour peer / Tick.

#include "runtime/core/log/log_system.h"
#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/script/dotnet_host.h"
#include "runtime/function/script/scene_behaviour_mount.h"

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

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
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

  ensureLogger();
  ObjectDB::clear();
  ClassDB::initialize();

  const std::filesystem::path host_dir = scriptHostDir();
  const std::filesystem::path dll = host_dir / "Blunder.ScriptHost.dll";
  const std::filesystem::path runtimeconfig =
      host_dir / "Blunder.ScriptHost.runtimeconfig.json";
  const std::filesystem::path game_dll = gameAssemblyPath();

  BlunderNativeAbi native_abi{};
  blunder_native_abi_fill_from_process(&native_abi);
  expect_true("process abi object_create",
              native_abi.object_create != nullptr);

  DotNetHost host;
  eastl::string err;
  const bool started = host.start(dll, runtimeconfig, native_abi, err);
  if (!started) {
    std::fprintf(stderr, "start error: %s\n", err.c_str());
  }
  expect_true("start succeeds", started);
  expect_true("isRunning", host.isRunning());

  bool loaded = false;
  if (started) {
    loaded = host.loadGameAssembly(game_dll, err);
    if (!loaded) {
      std::fprintf(stderr, "loadGameAssembly error: %s\n", err.c_str());
    }
  }
  expect_true("load game", loaded);

  const char* kJson = R"({
  "type": "Scene",
  "guid": "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff",
  "entities": [
    {
      "name": "Actor",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees",
      "behaviours": [
        {
          "type": "DotnetHostGame.ProbeBehaviour",
          "id": 1,
          "properties": {
            "EnabledFlag": true,
            "Speed": 1.5,
            "Label": "hi"
          }
        }
      ]
    }
  ]
}
)";

  if (started && loaded) {
    const bool resolved = host.resolveProbeTickCount(dll, err);
    if (!resolved) {
      std::fprintf(stderr, "resolveProbeTickCount error: %s\n", err.c_str());
    }
    expect_true("resolve probe tick count", resolved);

    // Object-slot bag overrides scene declaration when both diverge.
    Scene scene_bag;
    expect_true("deserialize bag-wins scene",
                SceneSerializer::deserialize(eastl::string(kJson), scene_bag));
    SceneInstance instance_bag;
    instance_bag.instantiate(scene_bag);
    const EntityId actor_bag_id = instance_bag.findEntityByName("Actor");
    expect_true("bag-wins actor entity", isValid(actor_bag_id));
    Object* actor_bag = ObjectDB::findByEntityId(actor_bag_id);
    expect_true("bag-wins actor Object bound", actor_bag != nullptr);
    if (actor_bag != nullptr) {
      eastl::vector<SceneBehaviourProperty> bag;
      SceneBehaviourProperty enabled;
      enabled.key = "EnabledFlag";
      enabled.value = Variant(false);
      bag.push_back(enabled);
      SceneBehaviourProperty speed;
      speed.key = "Speed";
      speed.value = Variant(9.0f);
      bag.push_back(speed);
      SceneBehaviourProperty label;
      label.key = "Label";
      label.value = Variant(eastl::string("bag-wins"));
      bag.push_back(label);
      expect_true("set object bag over scene decl",
                  actor_bag->setBehaviourProperties(static_cast<BehaviourId>(1),
                                                    bag));

      mountSceneBehaviours(instance_bag, host, &scene_bag);
      expect_true(
          "bag-wins peer non-null after mount",
          actor_bag->getBehaviourScriptPeer(static_cast<BehaviourId>(1)) !=
              nullptr);
      LifecycleDispatch::invokeReady(actor_bag);

      const int bag_wins = host.getProbeObjectBagWinsOk();
      if (bag_wins != 1) {
        std::fprintf(stderr, "getProbeObjectBagWinsOk=%d (expected 1)\n",
                     bag_wins);
      }
      expect_true("object bag over scene decl on mount", bag_wins == 1);
      expect_true("scene decl values not applied",
                  host.getProbePropertyOk() == 0);
    }
    instance_bag.clear();

    // Scene declaration property bag applied on mount.
    Scene scene;
    expect_true("deserialize scene",
                SceneSerializer::deserialize(eastl::string(kJson), scene));
    SceneInstance instance;
    instance.instantiate(scene);

    const EntityId actor_id = instance.findEntityByName("Actor");
    expect_true("actor entity", isValid(actor_id));
    Object* actor = ObjectDB::findByEntityId(actor_id);
    expect_true("actor Object bound", actor != nullptr);
    if (actor != nullptr) {
      expect_true("one slot before mount", actor->getBehaviourCount() == 1);
      expect_true("slot id 1",
                  actor->getBehaviourIdAt(0) == static_cast<BehaviourId>(1));
      expect_true("peer null before mount",
                  actor->getBehaviourScriptPeer(static_cast<BehaviourId>(1)) ==
                      nullptr);
    }

    mountSceneBehaviours(instance, host, &scene);

    if (actor != nullptr) {
      expect_true(
          "peer non-null after mount",
          actor->getBehaviourScriptPeer(static_cast<BehaviourId>(1)) != nullptr);
      expect_true("still one slot after mount (stable id)",
                  actor->getBehaviourCount() == 1);
      expect_true("stable BehaviourId 1",
                  actor->getBehaviourIdAt(0) == static_cast<BehaviourId>(1));

      LifecycleDispatch::invokeReady(actor);
      LifecycleDispatch::invokeTick(actor, 0.016f);
    }

    const int ticks = host.getProbeTickCount();
    if (ticks != 1) {
      std::fprintf(stderr, "getProbeTickCount=%d (expected 1)\n", ticks);
    }
    expect_true("managed Tick after scene mount", ticks == 1);

    const int props_ok = host.getProbePropertyOk();
    if (props_ok != 1) {
      std::fprintf(stderr, "getProbePropertyOk=%d (expected 1)\n", props_ok);
    }
    expect_true("property bag applied to ProbeBehaviour", props_ok == 1);

    instance.clear();
  }

  host.shutdown();
  ClassDB::shutdown();
  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "scene_behaviour_mount_test OK\n");
  return 0;
}
