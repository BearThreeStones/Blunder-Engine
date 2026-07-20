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
