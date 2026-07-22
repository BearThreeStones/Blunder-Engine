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
