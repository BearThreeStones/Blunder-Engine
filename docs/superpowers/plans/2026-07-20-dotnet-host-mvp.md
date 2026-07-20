# .NET Host MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship an in-process CoreCLR script host that loads one Project `Scripts/` assembly, attaches Unity-style Behaviours to Objects, and runs Ready/Tick in list order — without hot reload, scene persistence, or Inspector UX.

**Architecture:** Extend Object with an ordered Behaviour list (`BehaviourId` + type name + Script Peer). Native `LifecycleDispatch` iterates peers and calls type-level hooks. A thin **Blunder.ScriptHost** managed assembly (loaded via `nethost`/`hostfxr`) owns GCHandles and exposes `UnmanagedCallersOnly` entry points; game code references **Blunder.Api** (`net10.0`) and talks to the engine only through the existing C-ABI. Editor/dev path invokes `dotnet build` on `Scripts/` then loads the output DLL.

**Tech Stack:** MSVC / CMake `engine_runtime`, CoreCLR via `nethost` + `hostfxr` (.NET 10), C# `net10.0` class libraries, existing ClassDB / C-ABI / ObjectDB.

**Decisions (grilled):** ADR `0011-csharp-behaviour-scripting.md`, `CONTEXT.md` (§ Gameplay scripting / .NET host MVP / Scripts root). Reflection kernel is closed (ADR 0005); multi-Behaviour storage starts here.

**OpenSpec:** No change folder yet — create `openspec/changes/dotnet-host-mvp/` before `/opsx:apply` if you want spec gating. This plan is the implementation source of truth until that exists.

## Global Constraints

- TFM: `net10.0` (LTS) for Blunder.Api, Blunder.ScriptHost, and Create `Scripts/` templates
- Host: in-process CoreCLR via `nethost` / `hostfxr` — not Mono, not out-of-process `dotnet` IPC
- Interop: managed code uses C-ABI only — no C++ member pointers / direct P/Invoke of C++ methods
- Out of MVP: ALC hot reload, Behaviour scene serialization, Inspector Behaviour UX, NuGet publish, file-watcher auto-build, DogWalk content itself
- Product binaries: `bin/<Config>/`; tests: `tests/<Config>/` (see `docs/agents/build.md`)
- Build preset: `vs2026-debug` / `Debug`

---

## File map

| File | Responsibility |
|------|----------------|
| `engine/src/runtime/core/object/behaviour_id.h` | `BehaviourId` generational/monotonic handle helpers |
| `engine/src/runtime/core/object/object.h/.cpp` | Ordered Behaviour list; replace single `m_script_peer` |
| `engine/src/runtime/core/object/object_db.h/.cpp` | `forEach` / visit occupied Objects (for world Tick) |
| `engine/src/runtime/core/reflection/lifecycle.h/.cpp` | Invoke Ready/Tick **per Behaviour peer** in list order |
| `engine/src/runtime/core/reflection/engine_c_abi.h/.cpp` | C-ABI: attach/detach Behaviour, peer set, optional TRS get/set |
| `engine/src/runtime/function/script/dotnet_host.h/.cpp` | Load hostfxr, start ScriptHost, load game assembly, shutdown |
| `engine/src/runtime/function/script/scripts_builder.h/.cpp` | `dotnet build` on Project `Scripts/` |
| `engine/src/runtime/project/project_create.cpp` | Scaffold `Scripts/` + minimal `.csproj` |
| `engine/managed/Blunder.Api/` | Hand-authored MVP API: `Behaviour`, `ObjectHandle`, P/Invoke of C-ABI |
| `engine/managed/Blunder.ScriptHost/` | Loaded by native host; GCHandle table; `UnmanagedCallersOnly` exports |
| `engine/src/tests/behaviour_list_test.cpp` | Native multi-Behaviour + lifecycle (no .NET) |
| `engine/src/tests/dotnet_host_test.cpp` | Host smoke with fixture game assembly |
| `engine/src/tests/fixtures/dotnet_host_game/` | Tiny `net10.0` game used only by `dotnet_host_test` |
| `engine/src/runtime/CMakeLists.txt` | Wire native sources; copy `Blunder.Api.dll` beside `bin/` |
| `engine/src/tests/CMakeLists.txt` | New test targets |
| `docs/agents/testing.md` | Document new tests + .NET SDK requirement |

**Out of scope files:** Slint Inspector, scene serializer Behaviour fields, ALC unload paths.

```
┌─────────────┐     C-ABI      ┌──────────────────┐    load     ┌──────────────┐
│ engine_runtime│◄────────────►│ Blunder.ScriptHost│◄───────────│ Scripts/*.dll │
│ Object + list │              │ GCHandle + Tick   │            │ : Behaviour  │
│ Lifecycle    │              │ UnmanagedCallers  │            │   subclasses │
└──────┬───────┘              └─────────┬────────┘            └──────▲───────┘
       │ hostfxr/nethost                │ references                  │ HintPath
       ▼                                ▼                             │
┌─────────────┐              ┌──────────────────┐                     │
│ DotNetHost  │              │   Blunder.Api     │─────────────────────┘
└─────────────┘              └──────────────────┘
```

---

### Task 1: BehaviourId + multi-Behaviour list on Object (native)

**Files:**
- Create: `engine/src/runtime/core/object/behaviour_id.h`
- Modify: `engine/src/runtime/core/object/object.h`
- Modify: `engine/src/runtime/core/object/object.cpp` (add methods; create file if missing — today most Object methods may live in a single cpp; grep and put new methods beside existing Object TRS impl)
- Modify: `engine/src/runtime/CMakeLists.txt` (add `behaviour_id.h` if headers are listed; usually headers need not be in `add_library` but keep consistency)
- Test: `engine/src/tests/behaviour_list_test.cpp` (create in this task)
- Modify: `engine/src/tests/CMakeLists.txt`
- Modify: existing tests that call `setScriptPeer` / `getScriptPeer` / `clearScriptPeer` (`object_db_test.cpp`, `ptrcall_lifecycle_test.cpp`, `engine_c_abi_test.cpp`) to use the new Behaviour APIs or compatibility wrappers

**Interfaces:**
- Produces:
  - `using BehaviourId = uint64_t;` + `k_invalid_behaviour_id = 0`
  - `struct BehaviourSlot { BehaviourId id; eastl::string type_name; void* script_peer; };`
  - `BehaviourId Object::addBehaviour(eastl::string type_name);`
  - `bool Object::removeBehaviour(BehaviourId id);`
  - `size_t Object::getBehaviourCount() const;`
  - `BehaviourId Object::getBehaviourIdAt(size_t index) const;`
  - `const char* Object::getBehaviourTypeName(BehaviourId id) const;`
  - `void Object::setBehaviourScriptPeer(BehaviourId id, void* peer);`
  - `void* Object::getBehaviourScriptPeer(BehaviourId id) const;`
  - Compatibility: `getScriptPeer` / `setScriptPeer` / `clearScriptPeer` operate on **index 0** only (or no-op if empty) so old tests can migrate in the same task

- [ ] **Step 1: Write the failing test**

Create `engine/src/tests/behaviour_list_test.cpp`:

```cpp
#include "runtime/core/object/object_db.h"

#include <cstdio>

namespace {
int g_failures = 0;
void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}
}  // namespace

int main() {
  using namespace Blunder;
  ObjectDB::clear();
  ObjectId oid = ObjectDB::create();
  Object* object = ObjectDB::get(oid);
  expect_true("object", object != nullptr);
  if (object == nullptr) {
    return 1;
  }

  expect_true("empty", object->getBehaviourCount() == 0);
  BehaviourId a = object->addBehaviour("Motor");
  BehaviourId b = object->addBehaviour("Bark");
  BehaviourId c = object->addBehaviour("Motor");  // duplicate type allowed
  expect_true("count 3", object->getBehaviourCount() == 3);
  expect_true("ids distinct", a != b && b != c && a != c);
  expect_true("order 0 Motor",
              eastl::string(object->getBehaviourTypeName(a)) == "Motor");
  expect_true("order 1 Bark",
              eastl::string(object->getBehaviourTypeName(
                  object->getBehaviourIdAt(1))) == "Bark");

  int peer_a = 1;
  object->setBehaviourScriptPeer(a, &peer_a);
  expect_true("peer a", object->getBehaviourScriptPeer(a) == &peer_a);

  expect_true("remove b", object->removeBehaviour(b));
  expect_true("count 2", object->getBehaviourCount() == 2);
  expect_true("b gone", object->getBehaviourScriptPeer(b) == nullptr);

  ObjectDB::destroy(oid);
  ObjectDB::clear();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
```

Wire in `engine/src/tests/CMakeLists.txt` the same way as `classdb_test` (link `engine_runtime`, PCH reuse, `add_test`).

- [ ] **Step 2: Run test to verify it fails**

```powershell
cmake --build build/vs2026-debug --config Debug --target behaviour_list_test
```

Expected: compile error — `addBehaviour` / `BehaviourId` unknown.

- [ ] **Step 3: Implement BehaviourId + Object list**

`behaviour_id.h`:

```cpp
#pragma once
#include <cstdint>
namespace Blunder {
using BehaviourId = uint64_t;
constexpr BehaviourId k_invalid_behaviour_id = 0u;
inline bool isValidBehaviourId(BehaviourId id) {
  return id != k_invalid_behaviour_id;
}
}  // namespace Blunder
```

On `Object`, replace `void* m_script_peer` with:

```cpp
struct BehaviourSlot {
  BehaviourId id{k_invalid_behaviour_id};
  eastl::string type_name;
  void* script_peer{nullptr};
};
eastl::vector<BehaviourSlot> m_behaviours;
BehaviourId m_next_behaviour_id{1};
```

Implement `addBehaviour` by pushing a slot with `m_next_behaviour_id++`. Implement lookup helpers by scanning `m_behaviours`. Map legacy `setScriptPeer` to `m_behaviours[0]` (create a placeholder slot `"__legacy__"` only if you must keep old tests green without edits — prefer updating the three old tests instead).

- [ ] **Step 4: Run test to verify it passes**

```powershell
cmake --build build/vs2026-debug --config Debug --target behaviour_list_test
.\build\vs2026-debug\tests\Debug\behaviour_list_test.exe
```

Expected: exit 0.

- [ ] **Step 5: Update legacy Script Peer tests + rebuild**

Update `ptrcall_lifecycle_test.cpp` / `engine_c_abi_test.cpp` / `object_db_test.cpp` to `addBehaviour` + `setBehaviourScriptPeer` (or keep wrappers). Rebuild:

```powershell
cmake --build build/vs2026-debug --config Debug --target behaviour_list_test ptrcall_lifecycle_test object_db_test engine_c_abi_test
.\build\vs2026-debug\tests\Debug\behaviour_list_test.exe
.\build\vs2026-debug\tests\Debug\ptrcall_lifecycle_test.exe
.\build\vs2026-debug\tests\Debug\object_db_test.exe
.\build\vs2026-debug\tests\Debug\engine_c_abi_test.exe
```

Expected: all exit 0.

- [ ] **Step 6: Commit**

```bash
git add engine/src/runtime/core/object engine/src/tests/behaviour_list_test.cpp engine/src/tests/CMakeLists.txt engine/src/tests/ptrcall_lifecycle_test.cpp engine/src/tests/object_db_test.cpp engine/src/tests/engine_c_abi_test.cpp engine/src/runtime/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(script): add multi-Behaviour list on Object

Replace the single Script Peer slot with an ordered BehaviourId list so the
.NET host MVP can attach multiple C# Behaviours per Object.
EOF
)"
```

---

### Task 2: LifecycleDispatch ticks every Behaviour peer

**Files:**
- Modify: `engine/src/runtime/core/reflection/lifecycle.cpp`
- Modify: `engine/src/runtime/core/reflection/lifecycle.h` (comment only if signatures stay)
- Modify: `engine/src/tests/behaviour_list_test.cpp` (extend) or `ptrcall_lifecycle_test.cpp`
- Modify: `engine/src/runtime/core/object/object_db.h/.cpp` — add `forEachObject(Fn)` for later world tick

**Interfaces:**
- Consumes: `Object::getBehaviourCount`, `getBehaviourIdAt`, `getBehaviourScriptPeer`
- Produces: `LifecycleDispatch::invokeTick/Ready` call the registered hook once per non-null peer in list order; `ObjectDB::forEach(void (*)(Object*))` or template visitor

- [ ] **Step 1: Extend failing assertions in behaviour_list_test**

Append to `main` after peers exist:

```cpp
  int ticks = 0;
  auto on_tick = [](void* peer, float dt) {
    *static_cast<int*>(peer) += 1;
    (void)dt;
  };
  // Use a shared counter via peers:
  int peer0 = 0;
  int peer1 = 0;
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer0);
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(1), &peer1);
  LifecycleDispatch::clear();
  LifecycleDispatch::setTickHook("Object", on_tick);
  LifecycleDispatch::invokeTick(object, 0.016f);
  expect_true("both peers ticked", peer0 == 1 && peer1 == 1);
```

Include `lifecycle.h`.

- [ ] **Step 2: Run to see failure**

```powershell
cmake --build build/vs2026-debug --config Debug --target behaviour_list_test
.\build\vs2026-debug\tests\Debug\behaviour_list_test.exe
```

Expected: FAIL `both peers ticked` (only first peer or none).

- [ ] **Step 3: Implement multi-peer invoke**

In `lifecycle.cpp`:

```cpp
void LifecycleDispatch::invokeTick(Object* object, float delta_time) {
  if (object == nullptr) {
    return;
  }
  const auto it = hooks().find("Object");
  if (it == hooks().end() || it->second.tick == nullptr) {
    return;
  }
  const size_t n = object->getBehaviourCount();
  for (size_t i = 0; i < n; ++i) {
    const BehaviourId id = object->getBehaviourIdAt(i);
    void* peer = object->getBehaviourScriptPeer(id);
    if (peer == nullptr) {
      continue;
    }
    it->second.tick(peer, delta_time);
  }
}
```

Mirror for `invokeReady`. Add `ObjectDB::forEach`:

```cpp
template <typename Fn>
static void forEach(Fn&& fn) {
  // walk occupied slots; call fn(object*)
}
```

(Implement non-template in `.cpp` with `eastl::function<void(Object*)>` if you want to keep the header free of EASTL function — prefer a simple callback typedef for MVP.)

- [ ] **Step 4: Pass tests**

```powershell
cmake --build build/vs2026-debug --config Debug --target behaviour_list_test ptrcall_lifecycle_test
.\build\vs2026-debug\tests\Debug\behaviour_list_test.exe
.\build\vs2026-debug\tests\Debug\ptrcall_lifecycle_test.exe
```

Expected: exit 0.

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/core/reflection/lifecycle.cpp engine/src/runtime/core/object/object_db.* engine/src/tests/behaviour_list_test.cpp
git commit -m "$(cat <<'EOF'
feat(script): tick Ready/Tick for every Behaviour peer

LifecycleDispatch now walks the Object Behaviour list in order so multi-script
Objects match ADR 0011.
EOF
)"
```

---

### Task 3: C-ABI surface for Behaviours (+ Vec3 TRS for Api)

**Files:**
- Modify: `engine/src/runtime/core/reflection/engine_c_abi.h`
- Modify: `engine/src/runtime/core/reflection/engine_c_abi.cpp`
- Modify: `engine/src/tests/engine_c_abi_test.cpp`
- Modify: `openspec/specs/engine-c-abi/spec.md` (when OpenSpec change exists; otherwise skip and note in commit body)

**Interfaces:**
- Produces C API (keep `extern "C"`):

```c
typedef uint64_t BlunderBehaviourId;

BlunderBehaviourId blunder_object_add_behaviour(BlunderObjectId id,
                                                const char* type_name);
int blunder_object_remove_behaviour(BlunderObjectId id,
                                    BlunderBehaviourId behaviour_id);
int blunder_object_behaviour_count(BlunderObjectId id);
BlunderBehaviourId blunder_object_behaviour_id_at(BlunderObjectId id,
                                                  int index);
int blunder_object_set_behaviour_peer(BlunderObjectId id,
                                      BlunderBehaviourId behaviour_id,
                                      void* peer);
void* blunder_object_get_behaviour_peer(BlunderObjectId id,
                                        BlunderBehaviourId behaviour_id);

int blunder_object_set_vec3_property(BlunderObjectId id, const char* class_name,
                                     const char* property_name, float x,
                                     float y, float z);
int blunder_object_get_vec3_property(BlunderObjectId id, const char* class_name,
                                     const char* property_name, float* x,
                                     float* y, float* z);
```

Bump `BLUNDER_ENGINE_C_ABI_VERSION` to `2`.

- [ ] **Step 1: Write failing C-ABI assertions**

In `engine_c_abi_test.cpp` after create:

```cpp
  ClassDB::initialize();  // already present
  const BlunderBehaviourId bid =
      blunder_object_add_behaviour(id, "Motor");
  expect_true("add behaviour", bid != 0);
  expect_true("count", blunder_object_behaviour_count(id) == 1);
  int peer = 0;
  expect_true("set peer",
              blunder_object_set_behaviour_peer(id, bid, &peer) ==
                  BLUNDER_ENGINE_OK);
  expect_true("get peer",
              blunder_object_get_behaviour_peer(id, bid) == &peer);
  expect_true("set pos",
              blunder_object_set_vec3_property(id, "Object", "position", 1, 2,
                                              3) == BLUNDER_ENGINE_OK);
  float x = 0, y = 0, z = 0;
  expect_true("get pos",
              blunder_object_get_vec3_property(id, "Object", "position", &x, &y,
                                              &z) == BLUNDER_ENGINE_OK);
  expect_true("pos values", x == 1.f && y == 2.f && z == 3.f);
```

- [ ] **Step 2: Run — expect link/compile failure or FAIL assertions**

- [ ] **Step 3: Implement C-ABI wrappers** mapping to Object/ClassDB (Vec3 via `Variant(Vec3{x,y,z})`).

- [ ] **Step 4: Pass `engine_c_abi_test`**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_c_abi_test
.\build\vs2026-debug\tests\Debug\engine_c_abi_test.exe
```

Expected: exit 0; `blunder_engine_abi_version() == 2`.

- [ ] **Step 5: Commit**

```bash
git add engine/src/runtime/core/reflection/engine_c_abi.h engine/src/runtime/core/reflection/engine_c_abi.cpp engine/src/tests/engine_c_abi_test.cpp
git commit -m "$(cat <<'EOF'
feat(script): extend C-ABI for Behaviours and Vec3 properties

Version the ABI to 2 so Blunder.Api can attach peers and drive TRS without
C++ layouts.
EOF
)"
```

---

### Task 4: Hand-authored Blunder.Api (`net10.0`)

**Files:**
- Create: `engine/managed/Blunder.Api/Blunder.Api.csproj`
- Create: `engine/managed/Blunder.Api/Native.cs` (P/Invoke)
- Create: `engine/managed/Blunder.Api/Behaviour.cs`
- Create: `engine/managed/Blunder.Api/ObjectHandle.cs`
- Create: `engine/managed/Blunder.Api/Vec3.cs` (simple float3 struct matching C ABI)

**Interfaces:**
- Produces:

```csharp
namespace Blunder;

public readonly struct Vec3 { public float X, Y, Z; /* ctor */ }

public sealed class ObjectHandle {
  public ulong Id { get; }
  public Vec3 Position { get; set; }  // via Native get/set vec3
  public BehaviourId AddBehaviour<T>() where T : Behaviour, new();
  public T? GetBehaviour<T>() where T : Behaviour;
  public T[] GetBehaviours<T>() where T : Behaviour;
}

public abstract class Behaviour {
  public ObjectHandle Object { get; internal set; }
  public ulong BehaviourId { get; internal set; }
  public virtual void Ready() {}
  public virtual void Tick(float deltaTime) {}
}
```

`AddBehaviour<T>` must call native `blunder_object_add_behaviour` with `typeof(T).AssemblyQualifiedName` (or `FullName` if you pin assembly load context — use **FullName** for MVP and load from one game assembly only).

- [ ] **Step 1: Create csproj**

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
    <EnableDefaultCompileItems>true</EnableDefaultCompileItems>
    <AssemblyName>Blunder.Api</AssemblyName>
  </PropertyGroup>
</Project>
```

- [ ] **Step 2: Implement Native.cs P/Invoke** against `engine_editor` / test host module

For MVP tests, export C-ABI from a small **shared** DLL or link symbols into the test exe and pass `DllImport` library name. Practical approach on Windows:

1. Build `engine_runtime` as today (static).
2. Add `engine_c_abi_shared` **SHARED** library target that only compiles `engine_c_abi.cpp` + deps **or** export C-ABI from `dotnet_host_test` exe via `[UnmanagedCallersOnly]` inverse — simplest reliable MVP:

**Chosen approach:** create `blunder_engine_c` SHARED library:

```cmake
add_library(blunder_engine_c SHARED
  core/reflection/engine_c_abi.cpp
  # plus object_db, class_db, lifecycle, variant, object, generated — OR link engine_runtime PUBLIC and re-export
)
target_link_libraries(blunder_engine_c PRIVATE engine_runtime)
target_compile_definitions(blunder_engine_c PRIVATE BLUNDER_ENGINE_C_EXPORTS)
```

Decorate C functions with `__declspec(dllexport)` when `BLUNDER_ENGINE_C_EXPORTS` is set. Copy `blunder_engine_c.dll` next to managed test output.

`Native.cs`:

```csharp
internal static class Native {
  const string Lib = "blunder_engine_c";
  [DllImport(Lib)] public static extern int blunder_engine_abi_version();
  [DllImport(Lib)] public static extern ulong blunder_object_add_behaviour(ulong id, byte* typeUtf8);
  // ... remaining entry points with Utf8 marshalling
}
```

- [ ] **Step 3: Build Api**

```powershell
dotnet build engine/managed/Blunder.Api/Blunder.Api.csproj -c Debug
```

Expected: `Blunder.Api.dll` under `engine/managed/Blunder.Api/bin/Debug/net10.0/`.

- [ ] **Step 4: CMake copy Api beside `bin/<Config>/`**

After `dotnet build`, `add_custom_command` on `engine_editor` / `project_manager` copies:

- `Blunder.Api.dll`
- `blunder_engine_c.dll` (if separate)

Document path in Create template HintPath: `$(BlunderEngineDir)/Blunder.Api.dll` where `BlunderEngineDir` is the directory of `engine_editor.exe`.

- [ ] **Step 5: Commit**

```bash
git add engine/managed/Blunder.Api engine/src/runtime/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(script): add Blunder.Api net10.0 bindings skeleton

Hand-authored Behaviour/ObjectHandle surface over the versioned C-ABI for the
host MVP (Blueprint generator can replace stubs later).
EOF
)"
```

---

### Task 5: Blunder.ScriptHost + DotNetHost (nethost/hostfxr)

**Files:**
- Create: `engine/managed/Blunder.ScriptHost/Blunder.ScriptHost.csproj`
- Create: `engine/managed/Blunder.ScriptHost/HostExports.cs`
- Create: `engine/managed/Blunder.ScriptHost/PeerTable.cs`
- Create: `engine/src/runtime/function/script/dotnet_host.h`
- Create: `engine/src/runtime/function/script/dotnet_host.cpp`
- Modify: `engine/src/runtime/CMakeLists.txt` — find `nethost`, link `dotnet_host.cpp`, copy host policy / runtimeconfig
- Create: `engine/managed/Blunder.ScriptHost/Blunder.ScriptHost.runtimeconfig.json` (or generate from SDK)

**Interfaces:**
- Produces native:

```cpp
namespace Blunder {
class DotNetHost {
 public:
  bool start(const std::filesystem::path& script_host_dll,
             const std::filesystem::path& runtimeconfig,
             eastl::string& out_error);
  bool loadGameAssembly(const std::filesystem::path& game_dll,
                        eastl::string& out_error);
  bool attachBehaviour(ObjectId object, const char* clr_type_name,
                       BehaviourId* out_id, eastl::string& out_error);
  void shutdown();
  bool isRunning() const;
};
}
```

- Produces managed exports (exact names used by `hostfxr_get_runtime_delegate` / `load_assembly_and_get_function_pointer`):

```csharp
public static class HostExports {
  [UnmanagedCallersOnly]
  public static int LoadGameAssembly(IntPtr utf8Path);

  [UnmanagedCallersOnly]
  public static int AttachBehaviour(ulong objectId, IntPtr utf8TypeName,
                                    out ulong behaviourId);

  [UnmanagedCallersOnly]
  public static void RegisterLifecycleHooks();
}
```

`RegisterLifecycleHooks` calls `blunder_lifecycle_set_tick_hook` / `ready` with unmanaged function pointers that resolve `GCHandle` → `Behaviour.Tick/Ready`.

`PeerTable` maps `BehaviourId` ↔ `GCHandle` (normal handle). On attach: `Activator.CreateInstance`, set `Object`/`BehaviourId`, `GCHandle.Alloc`, `blunder_object_set_behaviour_peer`.

- [ ] **Step 1: Write DotNetHost test that fails without host**

Create `engine/src/tests/dotnet_host_test.cpp` that calls `DotNetHost{}.start(...)` and expects success — will fail to compile until Task 5 native exists.

- [ ] **Step 2: Implement ScriptHost C#**

Keep the project `EnableDynamicLoading` / produce `.runtimeconfig.json`. Reference `Blunder.Api`.

- [ ] **Step 3: Implement `dotnet_host.cpp`**

Follow Microsoft hosting pattern:

1. `get_hostfxr_path` from `nethost.h`
2. `hostfxr_initialize_for_runtime_config`
3. `hostfxr_get_runtime_delegate(hdt_load_assembly_and_get_function_pointer)`
4. Resolve `HostExports.LoadGameAssembly` etc. with `UNMANAGEDCALLERSONLY_METHOD`

CMake: locate .NET host pack (document `DOTNET_ROOT` / require SDK 10 on PATH). Link `nethost.lib` from the host pack.

- [ ] **Step 4: Manual smoke (before fixture automation)**

```powershell
dotnet build engine/managed/Blunder.ScriptHost/Blunder.ScriptHost.csproj -c Debug
cmake --build build/vs2026-debug --config Debug --target dotnet_host_test
```

Expected: links; start() returns true against ScriptHost dll path.

- [ ] **Step 5: Commit**

```bash
git add engine/managed/Blunder.ScriptHost engine/src/runtime/function/script engine/src/runtime/CMakeLists.txt engine/src/tests/dotnet_host_test.cpp engine/src/tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(script): embed CoreCLR via nethost and Blunder.ScriptHost

Load the managed script host in-process and resolve UnmanagedCallersOnly entry
points for game assembly load and Behaviour attach.
EOF
)"
```

---

### Task 6: Fixture game assembly + end-to-end Tick test

**Files:**
- Create: `engine/src/tests/fixtures/dotnet_host_game/DotnetHostGame.csproj`
- Create: `engine/src/tests/fixtures/dotnet_host_game/ProbeBehaviour.cs`
- Modify: `engine/src/tests/dotnet_host_test.cpp`
- Modify: `engine/src/tests/CMakeLists.txt` — `add_custom_command` to `dotnet build` fixture before test

**Interfaces:**
- `ProbeBehaviour : Behaviour` sets a static `public static int TickCount` on Tick
- Test creates Object via C-ABI/`ObjectDB`, starts host, loads fixture dll, `attachBehaviour(..., "DotnetHostGame.ProbeBehaviour")`, `LifecycleDispatch::invokeReady` + `invokeTick`, asserts `TickCount >= 1` via a small C-ABI or host export `GetProbeTickCount` **or** write count into a file — prefer managed export:

```csharp
[UnmanagedCallersOnly]
public static int GetProbeTickCount() => ProbeBehaviour.TickCount;
```

resolved by DotNetHost for the test only (acceptable MVP seam).

- [ ] **Step 1: Write ProbeBehaviour**

```csharp
namespace DotnetHostGame;

public sealed class ProbeBehaviour : Blunder.Behaviour {
  public static int TickCount;
  public override void Tick(float deltaTime) {
    TickCount++;
  }
}
```

csproj references `Blunder.Api` via HintPath to the built Api dll.

- [ ] **Step 2: Failing test asserts TickCount**

```cpp
  expect_true("host start", host.start(script_host_dll, runtimeconfig, err));
  expect_true("load game", host.loadGameAssembly(game_dll, err));
  BehaviourId bid{};
  expect_true("attach", host.attachBehaviour(oid, "DotnetHostGame.ProbeBehaviour", &bid, err));
  Object* o = ObjectDB::get(oid);
  LifecycleDispatch::invokeReady(o);
  LifecycleDispatch::invokeTick(o, 0.016f);
  expect_true("managed tick", host.getProbeTickCount() == 1);
```

- [ ] **Step 3: Implement until green**

```powershell
dotnet build engine/src/tests/fixtures/dotnet_host_game/DotnetHostGame.csproj -c Debug
cmake --build build/vs2026-debug --config Debug --target dotnet_host_test
.\build\vs2026-debug\tests\Debug\dotnet_host_test.exe
```

Expected: exit 0. Requires .NET 10 runtime installed.

- [ ] **Step 4: Document in `docs/agents/testing.md`**

Add row for `dotnet_host_test` and prerequisite: .NET 10 SDK + runtime.

- [ ] **Step 5: Commit**

```bash
git add engine/src/tests/fixtures/dotnet_host_game engine/src/tests/dotnet_host_test.cpp engine/src/tests/CMakeLists.txt docs/agents/testing.md
git commit -m "$(cat <<'EOF'
test(script): end-to-end CoreCLR Behaviour Tick smoke

Prove ScriptHost loads a fixture game assembly and LifecycleDispatch reaches
managed ProbeBehaviour.Tick.
EOF
)"
```

---

### Task 7: Scripts builder + Project Create scaffold

**Files:**
- Create: `engine/src/runtime/function/script/scripts_builder.h`
- Create: `engine/src/runtime/function/script/scripts_builder.cpp`
- Modify: `engine/src/runtime/project/project_create.cpp`
- Create: `engine/src/runtime/project/scripts_csproj_template.txt` (or embed string in cpp)
- Modify: `engine/src/tests/project_create_test.cpp` (assert `Scripts/` + `.csproj` exist)
- Optional: call `ScriptsBuilder::build` from editor Play path later — for MVP expose API + unit test with temp dir

**Interfaces:**

```cpp
struct ScriptsBuildResult {
  bool ok{false};
  eastl::string error;
  std::filesystem::path output_dll;
};

ScriptsBuildResult buildProjectScripts(const std::filesystem::path& project_root);
```

Implementation: `CreateProcessW` / `std::system` equivalent running:

```text
dotnet build "<root>/Scripts/<Name>.csproj" -c Debug -o "<root>/.blunder/scripts_bin"
```

Create scaffold writes:

```
Scripts/
  <ProjectName>.csproj   # net10.0, HintPath to Blunder.Api beside editor
  HelloBehaviour.cs      # empty Behaviour subclass (optional but recommended)
```

- [ ] **Step 1: Extend project_create_test expectations** for `Scripts` directory

- [ ] **Step 2: Fail, then implement scaffold + builder**

- [ ] **Step 3: Pass project_create_test**

```powershell
cmake --build build/vs2026-debug --config Debug --target project_create_test
.\build\vs2026-debug\tests\Debug\project_create_test.exe
```

- [ ] **Step 4: Commit**

```bash
git add engine/src/runtime/function/script/scripts_builder.* engine/src/runtime/project/project_create.cpp engine/src/tests/project_create_test.cpp
git commit -m "$(cat <<'EOF'
feat(project): scaffold Scripts/ and build game assemblies with dotnet

Create Projects ready for C# Behaviours and provide an editor-callable
dotnet build helper targeting .blunder/scripts_bin.
EOF
)"
```

---

### Task 8: Engine frame hooks DotNetHost Tick (minimal integration)

**Files:**
- Modify: `engine/src/runtime/function/global/global_context.h/.cpp` — own `eastl::unique_ptr<DotNetHost>`
- Modify: `engine/src/runtime/engine.cpp` — after scene tick, `ObjectDB::forEach` → `LifecycleDispatch::invokeTick` **only when host is running / play mode flag**
- Modify: `engine/src/editor/src/main.cpp` or editor launch — optional: auto-build+load Scripts when `--project-root` set and `.blunder/scripts_bin/*.dll` exists

**MVP play gate:** If full Play mode UI is missing, gate behind env `BLUNDER_DOTNET_SCRIPTS=1` or always load when `Scripts/` output exists — document the chosen gate in `docs/agents/testing.md`.

- [x] **Step 1: Add failing integration note test or manual checklist**

Manual checklist (acceptable for this task if automated UI Play is out of scope):

1. Create/open Project with Scripts scaffold
2. `dotnet build` via ScriptsBuilder
3. Start editor with project root
4. With host enabled, attach Probe-equivalent and observe log line from `Tick`

Prefer automating via `dotnet_host_test` already covering Tick; this task only wires production paths.

- [x] **Step 2: Implement global_context start/stop of DotNetHost**

- [x] **Step 3: Call lifecycle invoke from engine loop when host running**

```cpp
if (g_runtime_global_context.m_dotnet_host &&
    g_runtime_global_context.m_dotnet_host->isRunning()) {
  ObjectDB::forEach([&](Object* object) {
    LifecycleDispatch::invokeTick(object, delta_time);
  });
}
```

Call `invokeReady` once when Behaviours are first attached / on host load — track `bool m_ready_sent` per Behaviour slot if needed (add `bool ready_invoked` on `BehaviourSlot` in Task 1 follow-up).

- [x] **Step 4: Smoke with Test project**

```powershell
cmake --build build/vs2026-debug --config Debug --target engine_editor
# build Scripts for E:\Blunder Projects\Test if present
.\build\vs2026-debug\bin\Debug\engine_editor.exe --project-root "E:/Blunder Projects/Test"
```

Expected: no crash; host start errors are logged, not fatal, if Scripts missing.

- [x] **Step 5: Commit**

```bash
git add engine/src/runtime/function/global engine/src/runtime/engine.cpp docs/agents/testing.md
git commit -m "$(cat <<'EOF'
feat(script): drive Behaviour Tick from the engine frame

When DotNetHost is running, invoke LifecycleDispatch for every Object each
frame so Project Scripts participate in the runtime loop.
EOF
)"
```

---

## Self-review

**Spec coverage (ADR 0011 / CONTEXT .NET host MVP):**
| Requirement | Task |
|-------------|------|
| CoreCLR nethost host | 5 |
| Load one Project assembly | 5–6 |
| Multi Behaviour list + BehaviourId | 1 |
| Ready/Tick per peer order | 2, 6, 8 |
| Behaviour host Object + GetBehaviour | 4 |
| Scripts/ + Create scaffold | 7 |
| Editor `dotnet build` | 7 |
| Blunder.Api beside editor | 4 |
| net10.0 | 4–7 |
| No hot reload / serialization / Inspector | Explicitly out of all tasks |

**Placeholder scan:** No TBD/TODO steps; commands and types named.

**Type consistency:** `BehaviourId` = `uint64_t` / `ulong`; C-ABI version 2; hook table still keyed `"Object"` for MVP with per-peer GCHandle (virtual dispatch on managed `Behaviour`).

---

## Explore notes (stance)

```
Reflection kernel (done)          .NET host MVP (this plan)        Later
─────────────────────────         ─────────────────────────        ─────
ClassDB + C-ABI v1                Behaviour list + C-ABI v2        Serialization
Single peer skeleton              CoreCLR ScriptHost               ALC hot reload
Pilot TRS Variant                 Blunder.Api + Scripts/           Inspector UX
                                  Tick in engine loop              DogWalk content
```

**Risk spikes worth watching while implementing:**
- `nethost` discovery on developer machines without a global .NET 10 install
- Static `engine_runtime` vs needing a **DLL** for `DllImport` — Task 4’s `blunder_engine_c` SHARED is the load-bearing packaging choice
- Ready-once vs Ready-every-attach semantics (`BehaviourSlot.ready_invoked`)

**OpenSpec:** Want a formal change? Say the word and we can add `openspec/changes/dotnet-host-mvp/` (proposal/design/specs/tasks) mirroring this plan — explore mode did not create it yet.
