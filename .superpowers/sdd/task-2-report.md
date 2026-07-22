# Task 2 Report: C-ABI + NativeAbi v4

## Status: DONE

## TDD Evidence

### RED (Step 1–2)

Updated `native_abi_test.cpp` (four `message_*` non-null checks, `abi version >= 4`) and `engine_c_abi_test.cpp` (`blunder_engine_abi_version() == 4`).

```powershell
cmake --build build/vs2026-debug --config Debug --target native_abi_test
```

**Result:** exit 1 — compile errors:

```
native_abi_test.cpp(68,19): error C2039: "message_register": 不是 "BlunderNativeAbi" 的成员
native_abi_test.cpp(70,19): error C2039: "message_send": 不是 "BlunderNativeAbi" 的成员
native_abi_test.cpp(72,19): error C2039: "message_set_hook": 不是 "BlunderNativeAbi" 的成员
native_abi_test.cpp(74,19): error C2039: "message_clear_hook": 不是 "BlunderNativeAbi" 的成员
```

### GREEN (Step 3–4)

Implemented in `engine_c_abi.h` / `engine_c_abi.cpp`:

- `BLUNDER_ENGINE_C_ABI_VERSION` → 4
- `BlunderMessageArg` (uint8_t kind + 7-byte pad + union: b/i/f/object_id)
- `blunder_message_register`, `blunder_message_send`, `blunder_message_set_hook`, `blunder_message_clear_hook`
- `BlunderNativeAbi` fields + `fill_from_process` / `fill_from_module` LOAD macros
- Arg mapping helpers + hook adapter bridging `BlunderMessageHook` ↔ `MessageDispatch`

```powershell
cmake --build build/vs2026-debug --config Debug --target native_abi_test
.\build\vs2026-debug\engine\src\tests\Debug\native_abi_test.exe

cmake --build build/vs2026-debug --config Debug --target engine_c_abi_test
.\build\vs2026-debug\engine\src\tests\Debug\engine_c_abi_test.exe
```

**Result:** both exit 0.

## Commit

```
feat: expose Message register/send on C-ABI v4
```

## Test Summary

| Test | Result |
|------|--------|
| `native_abi_test` | PASS (process + module ABI completeness, version 4) |
| `engine_c_abi_test` | PASS (version 4, existing object/lifecycle coverage) |

## Concerns

- Managed `Blunder.Api` (`NativeAbi.cs`, `Native.cs`) not updated — deferred to Task 3.
- `BlunderMessageArg` layout uses explicit 7-byte padding for 8-byte union alignment; Task 3 must mirror field-for-field in C#.

## Files Changed

- `engine/src/runtime/core/reflection/engine_c_abi.h`
- `engine/src/runtime/core/reflection/engine_c_abi.cpp`
- `engine/src/tests/native_abi_test.cpp`
- `engine/src/tests/engine_c_abi_test.cpp`
