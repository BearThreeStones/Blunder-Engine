# Object Message Communication Implementation Plan



> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.



**Goal:** Add Harmon-style directed Messages so C# Behaviours can `Message.Register` / `Message.Send` to an ObjectId and every peer on that Object receives `OnMessage` synchronously.



**Architecture:** Native `MessageDispatch` owns name???MessageId registry and Send fan-out (BehaviourId snapshot, hook per peer). C-ABI / NativeAbi v4 exposes register/send/set_hook/clear_hook. ScriptHost registers an `OnMessage` hook beside Tick/Ready; managed `Message` fa??ade calls the registered pointers.



**Tech Stack:** C++20 `engine_runtime`, C-ABI / `BlunderNativeAbi`, C# `net10.0` Blunder.Api + ScriptHost, CMake `vs2026-debug`.



## Global Constraints



- Directed Message only; **no** Signal Connect/Emit in this slice

- Sync Send; fan-out to **all** Behaviour peers; **no** Handled-stops-siblings

- ??? args; wire kinds: Bool / Int64 / Float / ObjectId

- Game-registered names only; no engine Hit/Damage builtins

- Invalid ObjectId ???no-op (no throw)

- MVP callers: C#; native Send path exists but physics does not call it yet

- Bump `BLUNDER_ENGINE_C_ABI_VERSION` to **4**; NativeAbi pointer count **19 ???23**

- Product bins: `bin/<Config>/`; tests under `engine/src/tests/`; preset `vs2026-debug` / `Debug`

- OpenSpec: `openspec/changes/object-message-communication/`

- Glossary/ADR: `CONTEXT.md`, `docs/adr/0017-object-message-communication.md`



---



## File map



| File | Responsibility |

|------|----------------|

| `engine/src/runtime/core/reflection/message_dispatch.h` | MessageId, MessageArg, MessageDispatch API |

| `engine/src/runtime/core/reflection/message_dispatch.cpp` | Registry + send fan-out |

| `engine/src/runtime/core/reflection/engine_c_abi.h` | ABI v4, BlunderMessageArg, C exports, NativeAbi fields |

| `engine/src/runtime/core/reflection/engine_c_abi.cpp` | C wrappers + fill_from_process/module |

| `engine/src/runtime/CMakeLists.txt` | Add `message_dispatch.cpp` |

| `engine/src/tests/message_dispatch_test.cpp` | Native TDD |

| `engine/src/tests/native_abi_test.cpp` | Completeness + version ???4 |

| `engine/src/tests/engine_c_abi_test.cpp` | Expect ABI version 4 |

| `engine/src/tests/CMakeLists.txt` | `message_dispatch_test` target |

| `engine/managed/Blunder.Api/MessageArg.cs` | Managed arg kinds |

| `engine/managed/Blunder.Api/Message.cs` | Register / Send fa??ade |

| `engine/managed/Blunder.Api/Behaviour.cs` | `OnMessage` |

| `engine/managed/Blunder.Api/NativeAbi.cs` | Four new pointers |

| `engine/managed/Blunder.Api/Native.cs` | Completeness + wrappers |

| `engine/managed/Blunder.ScriptHost/HostExports.cs` | Message hook + clear |

| `engine/managed/Blunder.Api.NativeAbiTests/Program.cs` | Size 23; stub new entries |

| `engine/src/tests/editor_dotnet_host_test.cpp` or `object_message_dotnet_test.cpp` | Managed e2e Send |



```

Message.Register / Message.Send (C#)

        ???
        ???
NativeAbi message_*  ?????????MessageDispatch::register / send

                                ???
                                ???
                    snapshot BehaviourIds ???message hook(peer, id, args, argc)

                                ???
                                ???
                         Behaviour.OnMessage

```



---



### Task 1: Native MessageDispatch + unit tests



**Files:**

- Create: `engine/src/runtime/core/reflection/message_dispatch.h`

- Create: `engine/src/runtime/core/reflection/message_dispatch.cpp`

- Create: `engine/src/tests/message_dispatch_test.cpp`

- Modify: `engine/src/runtime/CMakeLists.txt` (add `core/reflection/message_dispatch.cpp` next to `lifecycle.cpp`)

- Modify: `engine/src/tests/CMakeLists.txt` (add target after `ptrcall_lifecycle_test` block)



**Interfaces:**

- Consumes: `ObjectDB`, `Object`, `BehaviourId`

- Produces:

  - `using MessageId = uint32_t;` (`0` = invalid)

  - `enum class MessageArgKind : uint8_t { Nil, Bool, Int, Float, ObjectId };`

  - `struct MessageArg { MessageArgKind kind; ... union ... };`

  - `using MessageHookFn = void (*)(void* peer, MessageId id, const MessageArg* args, int argc);`

  - `class MessageDispatch { static void clear(); static MessageId registerName(const char*); static void setHook(MessageHookFn); static bool send(ObjectId, MessageId, const MessageArg* args, int argc); };`

  - `send` returns `false` if `argc` not in `0..4` or `id==0`; invalid ObjectId returns `true` (no-op success)



- [ ] **Step 1: Write the failing test**



Create `engine/src/tests/message_dispatch_test.cpp`:



```cpp

#include "runtime/core/object/object_db.h"

#include "runtime/core/reflection/message_dispatch.h"



#include <cstdio>

#include <vector>



namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {

  if (!ok) {

    std::fprintf(stderr, "FAIL %s\n", label);

    ++g_failures;

  }

}



struct Call {

  void* peer;

  Blunder::MessageId id;

  int argc;

  Blunder::MessageArg args[4];

};

std::vector<Call> g_calls;



void Hook(void* peer, Blunder::MessageId id, const Blunder::MessageArg* args,

          int argc) {

  Call c{};

  c.peer = peer;

  c.id = id;

  c.argc = argc;

  for (int i = 0; i < argc && i < 4; ++i) {

    c.args[i] = args[i];

  }

  g_calls.push_back(c);

}

}  // namespace



int main() {

  Blunder::ObjectDB::clear();

  Blunder::MessageDispatch::clear();



  const Blunder::MessageId hit = Blunder::MessageDispatch::registerName("Hit");

  const Blunder::MessageId hit2 = Blunder::MessageDispatch::registerName("Hit");

  const Blunder::MessageId heal = Blunder::MessageDispatch::registerName("Heal");

  expect_true("register non-zero", hit != 0);

  expect_true("register stable", hit == hit2);

  expect_true("distinct names", hit != heal);



  Blunder::MessageDispatch::setHook(&Hook);

  const Blunder::ObjectId a = Blunder::ObjectDB::create();

  Blunder::Object* obj = Blunder::ObjectDB::get(a);

  expect_true("object", obj != nullptr);

  const Blunder::BehaviourId b0 = obj->addBehaviour("A");

  const Blunder::BehaviourId b1 = obj->addBehaviour("B");

  const Blunder::BehaviourId b2 = obj->addBehaviour("C");

  obj->setBehaviourScriptPeer(b0, reinterpret_cast<void*>(1));

  obj->setBehaviourScriptPeer(b1, nullptr);

  obj->setBehaviourScriptPeer(b2, reinterpret_cast<void*>(3));



  Blunder::MessageArg args[2];

  args[0].kind = Blunder::MessageArgKind::Int;

  args[0].i = 42;

  args[1].kind = Blunder::MessageArgKind::ObjectId;

  args[1].object_id = a;



  g_calls.clear();

  expect_true("send ok",

              Blunder::MessageDispatch::send(a, hit, args, 2));

  expect_true("two peers", g_calls.size() == 2);

  expect_true("order peer1", g_calls[0].peer == reinterpret_cast<void*>(1));

  expect_true("order peer3", g_calls[1].peer == reinterpret_cast<void*>(3));

  expect_true("argc", g_calls[0].argc == 2 && g_calls[0].args[0].i == 42);



  g_calls.clear();

  expect_true("invalid target ok",

              Blunder::MessageDispatch::send(0, hit, nullptr, 0));

  expect_true("invalid no calls", g_calls.empty());



  Blunder::MessageArg too_many[5]{};

  expect_true("argc 5 fails",

              !Blunder::MessageDispatch::send(a, hit, too_many, 5));

  expect_true("zero id fails",

              !Blunder::MessageDispatch::send(a, 0, nullptr, 0));



  if (g_failures != 0) {

    std::fprintf(stderr, "%d failure(s)\n", g_failures);

    return 1;

  }

  return 0;

}

```



- [ ] **Step 2: Run test to verify it fails**



```powershell

cmake --build build/vs2026-debug --config Debug --target message_dispatch_test

```



Expected: FAIL configure/compile ???`message_dispatch.h` missing (add CMake target first if needed so the missing header is the failure).



- [ ] **Step 3: Minimal MessageDispatch implementation**



`message_dispatch.h`:



```cpp

#pragma once



#include <cstdint>



#include "runtime/core/object/object_id.h"



namespace Blunder {



using MessageId = uint32_t;

inline constexpr MessageId k_invalid_message_id = 0;



enum class MessageArgKind : uint8_t {

  Nil = 0,

  Bool = 1,

  Int = 2,

  Float = 3,

  ObjectId = 4,

};



struct MessageArg {

  MessageArgKind kind{MessageArgKind::Nil};

  union {

    bool b;

    int64_t i;

    float f;

    ObjectId object_id;

  };

};



using MessageHookFn = void (*)(void* script_peer, MessageId id,

                               const MessageArg* args, int argc);



class MessageDispatch {

 public:

  static void clear();

  static MessageId registerName(const char* name);

  static void setHook(MessageHookFn fn);

  /// Returns false if id==0 or argc not in [0,4]. Invalid ObjectId is success no-op.

  static bool send(ObjectId target, MessageId id, const MessageArg* args,

                   int argc);

};



}  // namespace Blunder

```



`message_dispatch.cpp`: use `eastl::unordered_map<eastl::string, MessageId>`, monotonic id generator starting at 1, static hook pointer. In `send`: if `argc<0||argc>4||id==0` return false; `Object* o = ObjectDB::get(target)`; if null return true; copy `BehaviourId`s into a local `eastl::vector`; for each id re-get object/peer and call hook with args pointer (allow `args==nullptr` when argc==0).



- [ ] **Step 4: Wire CMake and run tests**



Add `core/reflection/message_dispatch.cpp` to `engine_runtime` sources. Add `message_dispatch_test` like `ptrcall_lifecycle_test` (link `engine_runtime`).



```powershell

cmake --build build/vs2026-debug --config Debug --target message_dispatch_test

.\build\vs2026-debug\engine\src\tests\Debug\message_dispatch_test.exe

```



Expected: exit 0.



- [ ] **Step 5: Commit**



```bash

git add engine/src/runtime/core/reflection/message_dispatch.h engine/src/runtime/core/reflection/message_dispatch.cpp engine/src/tests/message_dispatch_test.cpp engine/src/runtime/CMakeLists.txt engine/src/tests/CMakeLists.txt

git commit -m "$(cat <<'EOF'

feat: add MessageDispatch registry and sync fan-out



EOF

)"

```



---



### Task 2: C-ABI + NativeAbi v4



**Files:**

- Modify: `engine/src/runtime/core/reflection/engine_c_abi.h`

- Modify: `engine/src/runtime/core/reflection/engine_c_abi.cpp`

- Modify: `engine/src/tests/native_abi_test.cpp`

- Modify: `engine/src/tests/engine_c_abi_test.cpp` (expect version 4)



**Interfaces:**

- Consumes: `MessageDispatch`

- Produces C API:

  - `typedef uint32_t BlunderMessageId;`

  - `typedef struct BlunderMessageArg { uint8_t kind; ... } BlunderMessageArg;` (layout must match managed)

  - `typedef void (*BlunderMessageHook)(void* peer, BlunderMessageId id, const BlunderMessageArg* args, int argc);`

  - `int blunder_message_register(const char* name, BlunderMessageId* out_id);`

  - `int blunder_message_send(BlunderObjectId target, BlunderMessageId id, const BlunderMessageArg* args, int argc);`

  - `int blunder_message_set_hook(BlunderMessageHook hook);`

  - `int blunder_message_clear_hook(void);`

  - NativeAbi fields: `message_register`, `message_send`, `message_set_hook`, `message_clear_hook`

  - `#define BLUNDER_ENGINE_C_ABI_VERSION 4`



- [ ] **Step 1: Write failing completeness expectations**



In `native_abi_test.cpp` `expect_all_api_entries_non_null`, add checks for the four message fields; change `abi version >= 3` to `>= 4`. In `engine_c_abi_test.cpp`, expect `blunder_engine_abi_version() == 4`.



- [ ] **Step 2: Run to verify fail**



```powershell

cmake --build build/vs2026-debug --config Debug --target native_abi_test

.\build\vs2026-debug\engine\src\tests\Debug\native_abi_test.exe

```



Expected: FAIL missing fields / version.



- [ ] **Step 3: Implement C-ABI**



Map `BlunderMessageArg` ???`MessageArg` in cpp (same kind values). `register` writes out_id; `send` returns ERROR when MessageDispatch::send is false, else OK. `set_hook` / `clear_hook` wrap MessageDispatch. Extend `BlunderNativeAbi` and both fill helpers (LOAD macros for module path).



- [ ] **Step 4: Run tests**



```powershell

cmake --build build/vs2026-debug --config Debug --target native_abi_test

.\build\vs2026-debug\engine\src\tests\Debug\native_abi_test.exe

cmake --build build/vs2026-debug --config Debug --target engine_c_abi_test

.\build\vs2026-debug\engine\src\tests\Debug\engine_c_abi_test.exe

```



Expected: PASS.



- [ ] **Step 5: Commit**



```bash

git add engine/src/runtime/core/reflection/engine_c_abi.h engine/src/runtime/core/reflection/engine_c_abi.cpp engine/src/tests/native_abi_test.cpp engine/src/tests/engine_c_abi_test.cpp

git commit -m "$(cat <<'EOF'

feat: expose Message register/send on C-ABI v4



EOF

)"

```



---



### Task 3: Managed Api + ScriptHost hook



**Files:**

- Create: `engine/managed/Blunder.Api/MessageArg.cs`

- Create: `engine/managed/Blunder.Api/Message.cs`

- Modify: `engine/managed/Blunder.Api/Behaviour.cs`

- Modify: `engine/managed/Blunder.Api/NativeAbi.cs`

- Modify: `engine/managed/Blunder.Api/Native.cs`

- Modify: `engine/managed/Blunder.ScriptHost/HostExports.cs`

- Modify: `engine/managed/Blunder.Api.NativeAbiTests/Program.cs`



**Interfaces:**

- Consumes: NativeAbi message_* 

- Produces:

  - `public enum MessageArgKind : byte { Nil, Bool, Int, Float, ObjectId }`

  - `public struct MessageArg { ... static MessageArg FromInt(long); FromBool; FromFloat; FromObjectId; }`

  - `public readonly struct MessageId { public uint Value; }`

  - `public static class Message { static MessageId Register(string name); static void Send(ulong objectId, MessageId id, params MessageArg[] args); }`

  - `Behaviour.OnMessage(MessageId id, ReadOnlySpan<MessageArg> args)` virtual empty

  - ScriptHost: `OnMessage` unmanaged caller ???`behaviour.OnMessage`; register in `RegisterLifecycleHooks`; clear hook in `ShutdownCleanup`



- [ ] **Step 1: Update NativeAbiTests to expect 23 pointers (fails)**



Change `sizeof(BlunderNativeAbi) == 19 * sizeof(nint)` to `23 * sizeof(nint)`. Add stub function pointers for the four new fields in the complete abi. Build/run NativeAbiTests ???expect size fail until NativeAbi.cs updated.



- [ ] **Step 2: Extend NativeAbi + Native**



Add four `delegate* unmanaged[Cdecl]<...>` fields in the same order as C header. Extend `IsComplete`. Add wrappers `blunder_message_register`, `blunder_message_send`, `blunder_message_set_hook`, `blunder_message_clear_hook`.



- [ ] **Step 3: MessageArg + Message + Behaviour.OnMessage**



`Message.Register`: call native register, throw on Error. `Message.Send`: if args null treat as 0; if Length>4 throw `ArgumentException`; stackalloc/fixed `BlunderMessageArg` buffer; call send; throw on Error.



- [ ] **Step 4: ScriptHost hook**



```csharp

[UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]

static void OnMessage(IntPtr peer, uint id, BlunderMessageArg* args, int argc)

{

    if (peer == IntPtr.Zero) return;

    GCHandle handle = GCHandle.FromIntPtr(peer);

    if (handle.Target is not Behaviour behaviour) return;

    // copy to MessageArg[argc] then behaviour.OnMessage(new MessageId(id), span);

}



// In RegisterLifecycleHooks:

Native.blunder_message_set_hook((IntPtr)(delegate* unmanaged[Cdecl]<IntPtr, uint, BlunderMessageArg*, int, void>)&OnMessage);



// In ShutdownCleanup:

Native.blunder_message_clear_hook();

```



Define a managed `BlunderMessageArg` struct with `[StructLayout(LayoutKind.Sequential)]` matching C (put shared layout in Api).



- [ ] **Step 5: Build managed + NativeAbiTests**



```powershell

dotnet build engine/managed/Blunder.Api/Blunder.Api.csproj -c Debug

dotnet build engine/managed/Blunder.ScriptHost/Blunder.ScriptHost.csproj -c Debug

dotnet run --project engine/managed/Blunder.Api.NativeAbiTests -c Debug

```



Expected: exit 0.



- [ ] **Step 6: Commit**



```bash

git add engine/managed/Blunder.Api engine/managed/Blunder.ScriptHost engine/managed/Blunder.Api.NativeAbiTests

git commit -m "$(cat <<'EOF'

feat: add Message fa??ade and Behaviour.OnMessage hook



EOF

)"

```



---



### Task 4: DotNet integration smoke



**Files:**

- Modify: `engine/src/tests/fixtures/dotnet_host_game/` (add `MessageProbeBehaviour.cs` with static counter)

- Modify: `engine/src/tests/editor_dotnet_host_test.cpp` **or** create `object_message_dotnet_test.cpp` mirroring editor host pattern

- Modify: `engine/managed/Blunder.ScriptHost/HostExports.cs` (optional test seam `GetMessageProbeCount`)

- Modify: CMake test target as needed



**Interfaces:**

- Consumes: Tasks 1???

- Produces: e2e assertion that Send from native C-ABI (or managed via host) increments probe OnMessage count on a second Object



- [ ] **Step 1: Fixture Behaviour**



```csharp

namespace DotnetHostGame;

public class MessageProbeBehaviour : Behaviour

{

    public static int MessageCount;

    public static uint LastId;

    public override void OnMessage(MessageId id, ReadOnlySpan<MessageArg> args)

    {

        ++MessageCount;

        LastId = id.Value;

    }

}

```



Reset counter in test setup (static field = 0 before Attach).



- [ ] **Step 2: Failing e2e test**



Create two ObjectIds via process ABI, AttachBehaviour MessageProbe on target, Register `"Ping"`, Send from test (C-ABI `blunder_message_send` after hooks registered), assert MessageCount==1.



- [ ] **Step 3: Implement seams / fix until green**



```powershell

cmake --build build/vs2026-debug --config Debug --target editor_dotnet_host_test

.\build\vs2026-debug\engine\src\tests\Debug\editor_dotnet_host_test.exe

```



Expected: PASS (or dedicated `object_message_dotnet_test`).



- [ ] **Step 4: Commit**



```bash

git add engine/src/tests engine/managed/Blunder.ScriptHost

git commit -m "$(cat <<'EOF'

test: verify cross-Object Message reaches OnMessage



EOF

)"

```



---



## Self-review



1. **Spec coverage:** object-message (register/send/fan-out/OnMessage/fa??ade/args) ???Tasks 1???; csharp-behaviour hook ???Task 3; script-native-abi + engine-c-abi v4 ???Task 2; e2e ???Task 4.

2. **Placeholders:** none intentional; BlunderMessageArg exact C layout must be mirrored field-for-field when implementing (kind uint8_t + padding + union ???match MSVC/C# sequential carefully; prefer explicit `int64_t` storage for all payloads if padding fights you).

3. **Type consistency:** `MessageId` = `uint32_t` / managed `uint`; argc `int`; max 4; ABI v4; NativeAbi 23 pointers.



---



Plan complete and saved to `docs/superpowers/plans/2026-07-22-object-message-communication.md`. Two execution options:



**1. Subagent-Driven (recommended)** ???fresh subagent per task, review between tasks



**2. Inline Execution** ???execute tasks in this session with executing-plans checkpoints



Which approach?

