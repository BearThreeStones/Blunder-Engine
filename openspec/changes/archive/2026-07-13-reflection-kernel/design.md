## Context

Blunder today has `Entity` + side-car components inside `SceneInstance`, dense `EntityId`, YAML scene I/O, and hand-wired Inspector paths. There is no ClassDB, no export generator, and no C-ABI. Grilling + ADRs `0003`–`0005` lock dual-track Object/ECS, Clang export, PtrCall/Variant, and Reflection-kernel scope. Engine builds with MSVC (VS2026); Clang is a generator toolchain only.

## Goals / Non-Goals

**Goals:**
- Ship a working ClassDB with Clang-generated registration for a narrow pilot set
- Establish Object/`ObjectId`, property surface → Transform projection, lazy Entity materialization
- Provide PtrCall + small Variant and a C-ABI skeleton consumable by a future script host
- Keep Scene Tree ownership on Objects (projection to ECS when bound)

**Non-Goals:**
- .NET host, C# Source Generator productization, ALC hot reload
- Replacing `SceneInstance` with a full ECS World in this change
- Exporting all engine types; multi-Script-Peer; Component-as-Object

## Decisions

1. **Dual-track layout** — Object registry + thin ECS façade for pilot Transform storage (may wrap or mirror current entity TRS until full ECS lands). Prefer not to big-bang rewrite `SceneInstance` in this milestone; introduce Object alongside and bridge TRS through projected accessors.
2. **Generator** — Python/libclang (or equivalent) build tool scans annotated headers using a compilation database / clang flags; emits `.gen.cpp` + API Blueprint JSON. Daily engine compile stays MSVC.
3. **Markers** — Annotation macros (`CLASS`/`PROPERTY`/`FUNCTION` or project-equivalent) are metadata only; generated code owns registration.
4. **C-ABI** — Opaque `ObjectId` + method bind handles; `engine_ptrcall`-style entry; no C++ types in the public C header.
5. **Lifecycle** — Type-level hook table cleared/replaced as a unit; kernel implements the table and a no-op/default path until a host registers hooks.
6. **Pilot types** — `Object` (name, parent, enabled) + spatial properties projected to Transform; optionally one component schema type for MeshRenderer read-only metadata if time allows.

## Risks / Trade-offs

- **[Risk] Clang/MSVC flag drift → Mitigation:** Pin generator compile args; CI or script validates gen on PR when annotations change.
- **[Risk] Dual TRS during bridge period → Mitigation:** Single write path through Object property surface; no public API to edit both stores.
- **[Risk] Scope creep into ECS rewrite → Mitigation:** ADR 0005; tasks explicitly exclude SceneInstance replacement.
- **[Risk] C-ABI frozen too early → Mitigation:** Version the C header; keep blueprint as the evolution channel for bindings.

## Migration Plan

1. Land ClassDB + generator + empty/pilot registration
2. Introduce Object registry; bridge selected editor paths optionally later (not required for kernel merge)
3. Emit API Blueprint artifact in build output
4. Rollback: feature-flag or unused code paths; no scene format break required for kernel-only merge

## Open Questions

- Exact annotation attribute grammar beyond empty `BLUNDER_*()` macros (future exporter richness)
- ObjectId persistence format in scene files (generational handle vs UUID) — runtime uses generational `uint64` today
- `IEntityStore` SceneInstance adapter wiring in the editor (kernel uses the interface; full adapter can land with ECS migration)
