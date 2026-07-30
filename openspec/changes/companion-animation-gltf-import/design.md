## Context

Mesh Import already extracts AnimationClip YAML from animations **inside** the mesh Intermediate glTF (`extractAndRegisterAnimationClipsFromGltf`). DogWalk Chocomel splits mesh (`assets/char/chocomel/Chocomel.gltf`, skins=1, animations=0) from LOOP clips (`animations/world/LOOP-*/…`, skins=1, meshes=0, animations=1). Grilling locked Companion Animation glTF rules in CONTEXT and [ADR 0021](../../../docs/adr/0021-companion-animation-gltf-import.md). Playback (two-slot blend, TimeScale) stays in other changes.

## Goals / Non-Goals

**Goals:**
- Attach companions on mesh Import (multi-select primary; near-disk secondary).
- Acceptance: animations present, meshes empty; skins allowed.
- One skinned host per multi-select batch; copy companions under Resources; extract YAML clips under mesh stem; stem-based clip names.
- Reimport can re-extract from stored companion Intermediate bodies.
- Warn on bone-name mismatch; do not fail mesh Import.

**Non-Goals:**
- Godot AnimationLibrary / library/clip Play paths.
- Hard-coded `animations/world` (or any DogWalk-only) recursive discovery.
- Filename-fuzzy host↔companion pairing.
- Merging mesh+anims into one authored glTF as the required workflow.
- Changing AnimationPlayer runtime blend/TimeScale APIs.

## Decisions

1. **Multi-select primary** — Chocomel trees are disconnected; near-disk scan cannot find LOOP files. Alternatives rejected: DogWalk path hard-code; AnimationLibrary.
2. **Acceptance = animations ∧ meshes=0** — Measured on real LOOP files (skins=1). Rejected: skins=0 (would reject Chocomel); animations-only (would attach neighboring skinned characters).
3. **Companion Intermediate copy, not Mesh Asset** — Aligns with clip YAML as durable Intermediate; enables Reimport without external paths. Rejected: extract-only from absolute paths.
4. **Reuse existing clip extractor** — Point `extractAndRegisterAnimationClipsFromGltf` / refresh at each companion Intermediate; naming prefers file stem when processing companions.
5. **Batch pairing** — Exactly one skinned mesh host; orphans warn+skip. Rejected: fuzzy name match; two-step UI as MVP.

## Risks / Trade-offs

- [Near-disk scan false positives] → Mitigation: acceptance gate (meshes=0); skip full skinned neighbors.
- [Multi-select UX discoverability] → Mitigation: docs/checklist; Content Browser already multi-selects files.
- [Bone rename between mesh and LOOP] → Mitigation: warn+register; playback missing tracks same as today.
- [Main `openspec/specs/asset-import` still mentions COLLADA] → Mitigation: this change ADDs companion requirements; COLLADA cleanup is separate (ADR 0019 already shipped in code).

## Migration Plan

- No migration of existing Projects required; new Import behavior only.
- Authors re-Import Chocomel via multi-select to pick up companions.
- Rollback: feature-flag unnecessary; revert change restores prior extract-only-from-mesh behavior.

## Open Questions

- Exact Resources relative path for companion glTF bodies (e.g. under `Models/<meshStem>/companions/` vs beside mesh Intermediate) — implementer picks consistent convention; record in code comments / CONTENT_LAYOUT if needed.
- Whether mesh descriptor YAML records companion Intermediate virtual paths explicitly vs discovering under a fixed Resources prefix on Reimport — prefer explicit list on descriptor if cheap; else discover-by-convention.
