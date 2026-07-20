## 1. Native Behaviour list

- [x] 1.1 Add `BehaviourId` helpers and replace Object single Script Peer with ordered Behaviour slots
- [x] 1.2 Add `behaviour_list_test` covering add/remove, duplicate types, peer get/set
- [x] 1.3 Update legacy Script Peer tests (`object_db_test`, `ptrcall_lifecycle_test`, `engine_c_abi_test`) to Behaviour APIs

## 2. Lifecycle multi-peer dispatch

- [ ] 2.1 Change `LifecycleDispatch::invokeTick` / `invokeReady` to walk Behaviour peers in list order
- [ ] 2.2 Add `ObjectDB::forEach` (or equivalent) for world Tick
- [ ] 2.3 Extend tests so two peers both receive Tick; null peers are skipped
- [ ] 2.4 Track `ready_invoked` per Behaviour slot so Ready is not re-fired every frame

## 3. C-ABI v2

- [ ] 3.1 Add C-ABI Behaviour add/remove/count/id-at/set-peer/get-peer entry points
- [ ] 3.2 Add C-ABI Vec3 property get/set (at least Object position)
- [ ] 3.3 Bump `BLUNDER_ENGINE_C_ABI_VERSION` to 2 and extend `engine_c_abi_test`
- [ ] 3.4 Introduce SHARED `blunder_engine_c` (or equivalent) exporting C-ABI for managed `DllImport`

## 4. Blunder.Api (`net10.0`)

- [ ] 4.1 Create `engine/managed/Blunder.Api` with `Behaviour`, `ObjectHandle`, `Vec3`, P/Invoke of C-ABI
- [ ] 4.2 Implement `GetBehaviour` / `GetBehaviours` on the host Object handle
- [ ] 4.3 CMake: build Api and copy `Blunder.Api.dll` (+ `blunder_engine_c.dll`) beside `bin/<Config>/`

## 5. CoreCLR ScriptHost

- [ ] 5.1 Create `engine/managed/Blunder.ScriptHost` with GCHandle peer table and `UnmanagedCallersOnly` exports
- [ ] 5.2 Register Ready/Tick hooks that unbox GCHandle and call virtual `Behaviour` methods
- [ ] 5.3 Implement native `DotNetHost` (`nethost` / `hostfxr`) to start ScriptHost and resolve exports
- [ ] 5.4 Ensure host start failure is logged and non-fatal to editor/runtime startup

## 6. End-to-end Tick smoke

- [ ] 6.1 Add fixture game project `engine/src/tests/fixtures/dotnet_host_game` with `ProbeBehaviour`
- [ ] 6.2 Add `dotnet_host_test`: start host, load fixture, attach Behaviour, invoke Tick, assert managed side effect
- [ ] 6.3 Document .NET 10 SDK/runtime prerequisite in `docs/agents/testing.md`

## 7. Project Scripts scaffold and build

- [ ] 7.1 Extend `createProject` to scaffold `Scripts/` + minimal `net10.0` `.csproj` (HintPath to Blunder.Api)
- [ ] 7.2 Implement `ScriptsBuilder` invoking `dotnet build` into `.blunder/scripts_bin`
- [ ] 7.3 Update `project_create_test` for Scripts scaffold

## 8. Engine integration

- [ ] 8.1 Own `DotNetHost` on `RuntimeGlobalContext`; lazy-start when Scripts output exists (or documented gate)
- [ ] 8.2 From the engine frame, `forEach` Objects and `LifecycleDispatch::invokeTick` when host is running
- [ ] 8.3 Manual smoke: open/create Project, build Scripts, run editor without crash; Tick works for attached Behaviour

## 9. Docs alignment

- [ ] 9.1 Confirm ADR 0011 / CONTEXT / testing docs match shipped behavior
- [ ] 9.2 Cross-check OpenSpec tasks against `docs/superpowers/plans/2026-07-20-dotnet-host-mvp.md` (close any gaps)
