# Task 1 Report — Policy helper + unit test

**Status:** DONE  
**Date:** 2026-07-27  
**Branch:** feat/player-hide-editor-overlays  
**OpenSpec:** player-hide-editor-overlays

## Objective

Add pure `editorOverlaysEnabled(EngineHostMode)` policy helper with TDD unit test — foundation before OverlaySystem wiring.

## Files created

| Path | Purpose |
|------|---------|
| `engine/src/runtime/function/render/overlay/editor_overlay_policy.h` | Header-only policy: overlays enabled when host != Player |
| `engine/src/tests/editor_overlay_policy_test.cpp` | Unit tests for Editor/Player host modes |

## Files modified

| Path | Change |
|------|--------|
| `engine/src/tests/CMakeLists.txt` | Added `editor_overlay_policy_test` target after `play_pause_tick_gate_test` block |

## Interfaces delivered

```cpp
namespace Blunder {
inline bool editorOverlaysEnabled(EngineHostMode host_mode) {
  return host_mode != EngineHostMode::Player;
}
}
```

Consumes `EngineHostMode` from `runtime/function/global/engine_host_mode.h`. Pause is orthogonal — test documents Player stays disabled.

## TDD Evidence

### RED — test + CMake before header

**Command:**
```powershell
cmake --build build/vs2026-debug --config Debug --target editor_overlay_policy_test
```

**Output (excerpt):**
```
editor_overlay_policy_test.cpp(2,1): fatal error C1083: 无法打开包括文件: "runtime/function/render/overlay/editor_overlay_policy.h": No such file or directory
```

**Result:** Build failed — missing header (expected RED).

### GREEN — after policy header

**Build:**
```powershell
cmake --build build/vs2026-debug --config Debug --target editor_overlay_policy_test
```

**Output (excerpt):**
```
editor_overlay_policy_test.vcxproj -> E:\Dev\Blunder-Engine\build\vs2026-debug\engine\src\tests\Debug\editor_overlay_policy_test.exe
```

**Direct run:**
```powershell
.\build\vs2026-debug\engine\src\tests\Debug\editor_overlay_policy_test.exe
```
```
editor_overlay_policy_test: all passed
Exit code: 0
```

**CTest (from tests subdir — see Concerns):**
```powershell
ctest --test-dir build/vs2026-debug/engine/src/tests -C Debug -R editor_overlay_policy_test --output-on-failure
```
```
1/1 Test #33: editor_overlay_policy_test .......   Passed    0.01 sec
100% tests passed, 0 tests failed out of 1
```

## Test coverage

| Case | Expected |
|------|----------|
| Editor host | `editorOverlaysEnabled(Editor)` → true |
| Player host | `editorOverlaysEnabled(Player)` → false |
| Player + pause orthogonal | Player still false (documented in test comment) |

## Commits

| SHA | Message |
|-----|---------|
| `222054a` | test(overlay): add editorOverlaysEnabled host-mode policy |

## OpenSpec

Marked complete in `openspec/changes/player-hide-editor-overlays/tasks.md`: 1.1, 1.2, 1.3.

## Self-review

- Strict TDD order followed: failing test + CMake first, verified compile failure, then minimal header.
- Policy matches brief verbatim; no `engine_runtime` link (header-only + `engine_host_mode.h`).
- Test pattern matches other lightweight policy tests (`expect_true` / stderr on failure).
- No OverlaySystem or RenderSystem wiring (deferred to Tasks 2–3).
- MSVC C4819 warning on policy header (Unicode ellipsis/em dash in comment) — cosmetic only; matches brief text.

## Concerns

- `ctest --test-dir build/vs2026-debug` reports **Total Tests: 0** (no root `CTestTestfile.cmake`). Test is registered and passes via `ctest --test-dir build/vs2026-debug/engine/src/tests`. Same project-wide quirk as other engine tests; direct `.exe` run also passes.
