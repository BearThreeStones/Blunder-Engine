## Why

The .NET host MVP proved managed Behaviour Tick only against SHARED `blunder_engine_c`, which embeds its own `ObjectDB`/`ClassDB` statics. The editor’s scene Objects live in the process-linked `engine_runtime` image. ScriptHost/Api `DllImport("blunder_engine_c")` therefore mutates a second ObjectDB, so editor Scripts load stays gated and managed Tick on scene Objects cannot be claimed. Unifying the C-ABI target image is the blocker before real Project scripting and DogWalk C# work.

## What Changes

- Native host registers a **C-ABI function-pointer table** into managed ScriptHost/Api at start (same symbols as today’s C-ABI v2).
- Blunder.Api stops relying on process-wide `DllImport("blunder_engine_c")` for Object/Behaviour/lifecycle calls once the table is installed; all managed engine calls go through the registered pointers.
- Editor / runtime registers pointers from the **process-linked** C-ABI (`blunder_engine_c_static` / same ObjectDB as scene Objects).
- `dotnet_host_test` (Approach A) registers pointers from the **SHARED** `blunder_engine_c` it already LoadLibrary’s — tests keep a single ObjectDB without linking a second image.
- Remove or default-off the `BLUNDER_DOTNET_LOAD_SCRIPTS` dual-ObjectDB gate once editor path is proven; keep `BLUNDER_DOTNET_SCRIPTS` (or successor) as the Play/host-start gate until Play UI exists.
- Document the single-ObjectDB contract in `docs/agents/testing.md` and ADR 0011 follow-up note.
- **BREAKING** (managed interop only): Api that calls C-ABI before native registration fails clearly; hosts must register before `loadGameAssembly` / AttachBehaviour.

## Capabilities

### New Capabilities

- `script-native-abi`: Managed ScriptHost/Api consumes a native-registered C-ABI vtable so Object/Behaviour/lifecycle calls target exactly one ObjectDB per process.

### Modified Capabilities

- `dotnet-script-host`: Host start MUST install the C-ABI table before game assembly load; editor and test hosts use the process-appropriate image.
- `csharp-behaviour`: Managed Behaviours attached in the editor affect the same Objects the native frame Tick walks.
- `engine-c-abi`: Clarify that SHARED `blunder_engine_c` remains the DllImport/default export for standalone/test images, but in-process CoreCLR MUST NOT load a second ObjectDB alongside a statically linked `engine_runtime` consumer.

## Impact

- `engine/managed/Blunder.Api` (`Native.cs` and call sites)
- `engine/managed/Blunder.ScriptHost` (registration export + call order)
- `engine/src/runtime/function/script/dotnet_host.*` (pass/register table at start)
- `engine/src/runtime/core/reflection/engine_c_abi.*` (optional: typed table struct / fill helper)
- `engine/src/runtime/function/global/global_context.cpp` (drop Scripts-load gate after proof)
- `engine/src/tests/dotnet_host_test.cpp` (register SHARED exports into managed host)
- Docs: `docs/agents/testing.md`, ADR 0011 / CONTEXT glossary note
- Does **not** make `engine_runtime` SHARED; does **not** ship Behaviour serialization or Play UI
