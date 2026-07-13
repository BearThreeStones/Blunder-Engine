## Why

Blunder has no ClassDB, no export pipeline, and no stable C-ABI for editor or future C# scripting. Ad-hoc Inspector bindings and a later “pure C++ macro reflection” path would fight hot-reload-safe scripting. We need a Reflection kernel now so editor and scripts share one metadata source shaped for a flat C-ABI, without boiling the ocean into .NET host or full ECS rewrite.

## What Changes

- Introduce **ClassDB** as the single type database for exported classes, properties, methods, and component schemas
- Add **Clang AST** static export (narrow pilot set) producing ClassDB registration + **API Blueprint**
- Add **Object** / **ObjectId**, Object property surface with projected Transform accessors, and **lazy Entity materialization**
- Add **PtrCall** (primary) and a small **Variant** path (editor/serialization)
- Add **type-level lifecycle dispatch** tables (Ready/Tick slots) invoked via PtrCall
- Add a **C-ABI skeleton** for ClassDB/PtrCall/ObjectId operations
- Document dual-track Object + ECS boundaries (Scene Tree on Object; Component ≠ Object; ≤1 Script Peer)
- **Out of scope:** .NET/nethost, ALC hot reload, full `SceneInstance` → ECS World replacement, exporting the whole engine

## Capabilities

### New Capabilities
- `classdb`: Runtime type database, export markers, Clang generator outputs, PtrCall/Variant call paths
- `object-identity`: Object, ObjectId, Scene Tree ownership, optional Entity binding, lazy materialization, property surface
- `engine-c-abi`: Stable C-ABI surface for ClassDB/Object/PtrCall (script-host ready, host not shipped)

### Modified Capabilities
- *(none — no existing reflection/scripting specs)*

## Impact

- New runtime areas under `engine/src/runtime/` (e.g. `core`/`function` reflection, object registry) wired via CMake + `RuntimeGlobalContext` as needed
- New tools/scripts for Clang-based export (MSVC remains the engine compiler)
- Pilot exported types only (Object + Transform projection); today’s `Entity` C++ class remains until a later ECS migration
- ADRs: `docs/adr/0003-dual-track-object-and-ecs.md`, `0004-classdb-clang-export-and-call-paths.md`, `0005-reflection-kernel-mvp-scope.md`
- Glossary: `CONTEXT.md` (Reflection & scripting)
- Future consumers (not this change): C# Source Generator, nethost, ALC hot reload
