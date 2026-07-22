# Unify script ObjectDB (C-ABI function-pointer table)

> OpenSpec: `openspec/changes/unify-script-objectdb/`  
> Branch: `feat/unify-script-objectdb`

## Global Constraints

- TDD: failing test first for each behavior change; no silent DllImport fallback after registration lands.
- One ObjectDB per process for editor + managed ScriptHost/Api.
- Keep SHARED `blunder_engine_c` for Approach A `dotnet_host_test`; do not make `engine_runtime` SHARED.
- Editor registers process-linked C-ABI (`blunder_engine_c_static` symbols); tests register SHARED module exports.
- Commit after each plan task; keep diffs scoped; do not commit unrelated WIP (Assets/PM/etc.).
- Subagent model: `cursor-grok-4.5-high`.
- Mark matching checkboxes in `openspec/changes/unify-script-objectdb/tasks.md` when done.

---

### Task 1: Native ABI table (OpenSpec 1.1–1.3)

**Files:**
- Add/modify: `engine/src/runtime/core/reflection/engine_c_abi.h` / `.cpp` (or sibling `native_abi.h`)
- Add: unit test for fill helpers (e.g. `native_abi_test` or extend `engine_c_abi_test`)

**Steps (TDD):**
1. RED: test that `blunder_native_abi_fill_from_process` yields non-null pointers for every C-ABI v2 entry Blunder.Api uses; test module-fill from LoadLibrary SHARED when applicable.
2. GREEN: define `BlunderNativeAbi` POD matching Api entry points; implement `blunder_native_abi_fill_from_process()`; add `blunder_native_abi_fill_from_module` (Windows HMODULE / void* handle) for tests.
3. Wire CMake if new test target.
4. Commit: `feat(script): add BlunderNativeAbi fill helpers`

Mark OpenSpec `- [x]` for 1.1, 1.2, 1.3.

---

### Task 2: Managed registration path (OpenSpec 2.1–2.3)

**Files:**
- `engine/managed/Blunder.Api/Native.cs` (+ maybe `NativeAbi.cs`)
- `engine/managed/Blunder.ScriptHost/HostExports.cs`

**Steps (TDD):**
1. RED: managed or host test proving call before register fails; after `RegisterNativeAbi` succeed.
2. GREEN: `RegisterNativeAbi` UnmanagedCallersOnly stores table in Api; Native invokes function pointers; no silent DllImport.
3. Ensure ScriptHost ALC still shares Blunder.Api with game assemblies so registration is visible.
4. Commit: `feat(script): register native C-ABI table into Blunder.Api`

Mark OpenSpec 2.1–2.3.

---

### Task 3: DotNetHost wiring + Approach A test (OpenSpec 3.1–3.2)

**Files:**
- `engine/src/runtime/function/script/dotnet_host.cpp` / `.h`
- `engine/src/tests/dotnet_host_test.cpp`

**Steps (TDD):**
1. RED: `dotnet_host_test` fails until SHARED exports are registered before attach/Tick.
2. GREEN: DotNetHost registers table after ScriptHost load, before hooks / loadGameAssembly; test fills from SHARED module.
3. Commit: `feat(script): DotNetHost registers C-ABI before Scripts load`

Mark OpenSpec 3.1–3.2.

---

### Task 4: Editor ObjectDB proof + gate removal (OpenSpec 4.1–4.3)

**Files:**
- New test linking process ObjectDB + DotNetHost (editor-style)
- `global_context.cpp` — remove `BLUNDER_DOTNET_LOAD_SCRIPTS` gate
- `docs/agents/testing.md` (+ ADR/CONTEXT note if needed)

**Steps (TDD):**
1. RED: editor-style test: `ObjectDB::create` in process, register process ABI, attach Behaviour, `LifecycleDispatch::invokeTick`, assert managed side effect.
2. GREEN: implement test + remove dual-ObjectDB load gate; update docs.
3. Commit: `feat(script): single ObjectDB editor Scripts path`

Mark OpenSpec 4.1–4.3.

---

### Task 5: Verification (OpenSpec 5.1–5.2)

**Steps:**
1. Run `dotnet_host_test`, new editor-style test, `engine_c_abi_test` (and `native_abi_test` if added) — all green.
2. Manual smoke: editor `BLUNDER_DOTNET_SCRIPTS=1` + project Scripts loads without `BLUNDER_DOTNET_LOAD_SCRIPTS`; document result in report.
3. Mark OpenSpec 5.1–5.2; commit docs-only if any leftover: `test(script): verify unify-script-objectdb`

---

## Done when

- Editor-style test proves managed Tick on process ObjectDB
- `dotnet_host_test` still green (Approach A + registered SHARED table)
- `BLUNDER_DOTNET_LOAD_SCRIPTS` gate removed
- OpenSpec tasks.md all `[x]`
