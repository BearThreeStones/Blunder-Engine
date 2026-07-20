## Context

`.NET host MVP` shipped CoreCLR + Blunder.Api/ScriptHost + SHARED `blunder_engine_c` for `DllImport`. CMake already warns: do not link `blunder_engine_c_static` and SHARED `blunder_engine_c` in the same process — each links `engine_runtime` and embeds its own ObjectDB/ClassDB statics. The editor uses the static path for scene Objects; managed code DllImports the SHARED DLL → dual ObjectDB. Editor Scripts load is gated (`BLUNDER_DOTNET_LOAD_SCRIPTS`). Stakeholders: DotNetHost, Blunder.Api, ScriptHost, `dotnet_host_test`, future Play mode.

## Goals / Non-Goals

**Goals:**
- One ObjectDB per process for managed Behaviour attach + native frame Tick in the editor
- Keep Approach A `dotnet_host_test` green (SHARED-only Object traffic via registered pointers)
- Remove the dual-ObjectDB Scripts-load gate after automated proof
- Clear failure if managed code calls C-ABI before registration

**Non-Goals:**
- Making `engine_runtime` a SHARED library
- Removing SHARED `blunder_engine_c` (still needed for tests / future standalone)
- Behaviour serialization, Inspector UX, ALC hot reload, Play-mode UI
- Generating Blunder.Api from Blueprint

## Decisions

1. **Native-registered C-ABI function-pointer table (vtable)**  
   At DotNetHost start (after ScriptHost loads, before `loadGameAssembly`), native fills a POD table of all Blunder.Api C-ABI entry points and calls a ScriptHost `UnmanagedCallersOnly` export (e.g. `RegisterNativeAbi`) that stores the table in Blunder.Api. Subsequent Api calls invoke the pointers — not `DllImport("blunder_engine_c")`.  
   *Rejected:* Making `engine_runtime` SHARED (too large). Exporting C-ABI only from `engine_editor.exe` + `DllImport` of the main module (works on Windows with export-all, but couples Api library name to host binary and complicates tests). `NativeLibrary.SetDllImportResolver` remapping `"blunder_engine_c"` to an already-loaded module without a pointer table (still needs symbols exported from the EXE for the static path).

2. **Who fills the table**  
   - **Editor / runtime:** fill from process-linked symbols (`blunder_engine_c_static` / same TU as editor ObjectDB).  
   - **`dotnet_host_test`:** `LoadLibrary` SHARED `blunder_engine_c`, `GetProcAddress` each symbol, pass that table into ScriptHost (same registration path).  
   A small C helper `blunder_native_abi_fill_from_process()` (static) and optional `blunder_native_abi_fill_from_module(HMODULE)` (test) avoid duplicating symbol lists.

3. **DllImport fallback**  
   Keep DllImport attributes only as a **deprecated fallback** behind an explicit define/env for emergency debugging, OR remove them and hard-require registration. Prefer **hard-require**: calling before register throws / returns Error so dual-ObjectDB cannot silently return.  
   *Rejected:* Soft fallback to DllImport in editor (reintroduces the bug).

4. **Gate removal**  
   After an editor-oriented integration test (or extended `dotnet_host_test` pattern that proves attach+Tick against process ObjectDB) passes: remove `BLUNDER_DOTNET_LOAD_SCRIPTS` gate; keep `BLUNDER_DOTNET_SCRIPTS` (or auto-start when scripts_bin exists) as host-start policy until Play UI.

5. **ABI version**  
   No C-ABI version bump required if the exported C functions are unchanged; the registration table is a hosting concern. Optionally bump to v3 only if we add a C export for the table itself — prefer unmanaged registration from native without a new public C symbol set beyond the fill helper.

## Risks / Trade-offs

- **[Table drift vs Native.cs]** → Single generated or hand-maintained struct mirrored in C#; compile-time size assert; test that every Api entry is non-null after register.
- **[Forgot to register in a new host]** → Fail loud on first Native call; document in testing.md.
- **[SHARED still on PATH beside editor]** → Editor must not LoadLibrary it for Scripts; registration uses static symbols. Staging SHARED beside `bin/` for projects/tests remains OK if unmanaged code never loads it in the editor process for Object traffic.
- **[Perf]** → Indirect call per P/Invoke replacement; negligible vs managed Tick body.

## Migration Plan

1. Add `BlunderNativeAbi` struct + fill helpers; ScriptHost `RegisterNativeAbi`.
2. Switch Blunder.Api `Native` to pointer table; TDD via managed/native tests.
3. DotNetHost always registers before `RegisterLifecycleHooks` / `loadGameAssembly`.
4. Update `dotnet_host_test` to register SHARED exports.
5. Add editor-process proof (test or documented smoke): create Object on process ObjectDB, attach via host, Tick, assert.
6. Remove `BLUNDER_DOTNET_LOAD_SCRIPTS` gate; update testing.md / ADR note.
7. Rollback: restore DllImport + gate (feature flag) if critical regression.

## Open Questions

- Exact editor automated proof shape: prefer extending a CTest that links like the editor (static ObjectDB + DotNetHost) over manual-only smoke — **default: yes, add focused test**.
- Whether to delete DllImport attributes entirely vs leave unused — **default: delete call path; keep string only in comments**.
