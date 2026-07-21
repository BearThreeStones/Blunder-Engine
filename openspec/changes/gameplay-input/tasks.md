## 1. Native Action sampler (pure)

- [x] 1.1 Add `GameplayInputState` (keys → Move/Jump snapshot) under `engine/src/runtime/platform/input/`
- [x] 1.2 Add `gameplay_input_test` covering diagonal normalize, opposing cancel, Jump edge, shared edge, Pause discard, unfocused idle, non-Player idle
- [x] 1.3 Wire test target in `engine/src/tests/CMakeLists.txt`

## 2. C-ABI + NativeAbi table

- [x] 2.1 Add `blunder_gameplay_input_get_move` / `blunder_gameplay_input_was_jump_pressed` to `engine_c_abi.h/.cpp`
- [x] 2.2 Append fields to `BlunderNativeAbi`; bump `BLUNDER_ENGINE_C_ABI_VERSION` to 3; update `blunder_native_abi_fill_from_process` / `_from_module`
- [x] 2.3 Update `native_abi_test` completeness checks for the new pointers and version ≥ 3

## 3. Player frame sampling

- [x] 3.1 Sample SDL keyboard + window focus + Pause into `GameplayInputState` once per frame in Player host before Behaviour Tick
- [x] 3.2 Leave editor `InputSystem`/`GameCommand` unchanged as a separate path

## 4. Managed Api surface

- [x] 4.1 Extend `BlunderNativeAbi` / `Native` completeness + wrappers; update `Blunder.Api.NativeAbiTests` (layout size 19 pointers)
- [x] 4.2 Add `Vec2` + static `Input.GetMove` / `Input.WasJumpPressed`
- [x] 4.3 Add managed test coverage for Input façade (stub ABI or extend NativeAbiTests)

## 5. Docs / verify

- [x] 5.1 Confirm CONTEXT + ADR 0015 still match shipped names; note plan path if needed
- [x] 5.2 Run native `gameplay_input_test`, `native_abi_test`, managed NativeAbiTests, and Play-related ctest subset
