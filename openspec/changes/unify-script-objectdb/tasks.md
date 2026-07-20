## 1. Native ABI table

- [x] 1.1 Define `BlunderNativeAbi` (or equivalent) POD matching Blunder.Api C-ABI v2 entry points in `engine_c_abi` headers
- [x] 1.2 Add `blunder_native_abi_fill_from_process()` filling pointers from process-linked C-ABI symbols
- [x] 1.3 Add test helper to fill the table from a LoadLibrary’d SHARED `blunder_engine_c` module

## 2. Managed registration path

- [x] 2.1 Add ScriptHost `RegisterNativeAbi` `UnmanagedCallersOnly` export that stores the table for Blunder.Api
- [x] 2.2 Change Blunder.Api `Native` to call through the registered table; fail clearly if unregistered (no silent DllImport fallback)
- [x] 2.3 Resolve Blunder.Api from ScriptHost ALC so registration is visible to game assemblies (keep existing shared-Api ALC behavior)

## 3. DotNetHost wiring

- [ ] 3.1 After ScriptHost load, DotNetHost fills + registers the process table before lifecycle hook registration and `loadGameAssembly`
- [ ] 3.2 Update `dotnet_host_test` to register SHARED module exports (Approach A remains single-ObjectDB)

## 4. Editor ObjectDB proof + gate removal

- [ ] 4.1 Add automated test that links like the editor (process ObjectDB + DotNetHost): create Object, attach Behaviour, invokeTick, assert managed side effect
- [ ] 4.2 Remove `BLUNDER_DOTNET_LOAD_SCRIPTS` dual-ObjectDB gate from `global_context` Scripts load path
- [ ] 4.3 Update `docs/agents/testing.md` (and ADR 0011 / CONTEXT note if needed) for single-ObjectDB + remaining Play/host-start gate

## 5. Verification

- [ ] 5.1 `dotnet_host_test`, new editor-style test, `engine_c_abi_test` green
- [ ] 5.2 Manual smoke: editor with `BLUNDER_DOTNET_SCRIPTS=1` + Project Scripts loads assembly without `BLUNDER_DOTNET_LOAD_SCRIPTS`
