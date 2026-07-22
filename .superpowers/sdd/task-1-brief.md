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
