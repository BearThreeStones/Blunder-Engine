# Chocomel multi-select Import — Test Project

Manual validation steps for DogWalk Chocomel using **multi-select Import**. This
documents how to unblock Mesh + AnimationClip Assets for the split export layout.
It does **not** mark DogWalk animation Phase 1/2 Play acceptance Done — that
remains gated on Tasks 5.x and the separate `dogwalk-animation-phase-*` changes.

## Source files (dogwalk-repo)

Primary tree (`pro/game`):

| Role | Absolute path |
|------|---------------|
| Skinned mesh host (skins=1, animations=0) | `E:\Godot Projects\dogwalk-repo\pro\game\assets\char\chocomel\Chocomel.gltf` |
| LOOP idle companion (skins=1, meshes=0, animations=1) | `E:\Godot Projects\dogwalk-repo\pro\game\animations\world\LOOP-chocomel-idle\LOOP-chocomel-idle.gltf` |
| LOOP walk companion (skins=1, meshes=0, animations=1) | `E:\Godot Projects\dogwalk-repo\pro\game\animations\world\LOOP-chocomel-walk\LOOP-chocomel-walk.gltf` |

The same three files also exist under the `dogwalk-(4.4)` tree when that
worktree is checked out; paths are identical relative to `pro/game`.

**Why multi-select:** Chocomel mesh lives under `assets/char/…` while LOOP clips
live under `animations/world/…`. Near-disk auto-discovery only scans the mesh
directory and immediate child directories of its parent — it does **not**
recursively walk `animations/world`. Multi-select is the supported path for this
disconnected layout (ADR 0021).

## Prerequisites

- Open the Blunder **Test Project** (or any project with Content Browser Import).
- Mesh Import must have **animations enabled** (default for skinned glTF Import).

## Steps

1. In the OS file picker exposed by Content Browser **Import**, navigate to the
   dogwalk-repo paths above.
2. **Multi-select all three** glTF files (Chocomel mesh + both LOOP companions).
3. Choose destination folder `assets/Meshes` (or equivalent Meshes Asset folder).
4. Confirm Import.

## Expected results

After Import completes:

| Asset | Expected descriptor |
|-------|---------------------|
| Mesh | `assets/Meshes/Chocomel.mesh.yaml` |
| Idle clip | `assets/Animations/LOOP-chocomel-idle.animation.yaml` |
| Walk clip | `assets/Animations/LOOP-chocomel-walk.animation.yaml` |

Companion glTF bodies are copied under Resources (not registered as Mesh Assets):

- `resources/Models/Chocomel/companions/LOOP-chocomel-idle.gltf`
- `resources/Models/Chocomel/companions/LOOP-chocomel-walk.gltf`

The Mesh descriptor records both paths in `companion_animation_sources`.

Clip logical names prefer the **companion file stem** (`LOOP-chocomel-idle`,
`LOOP-chocomel-walk`). AnimationPlayer Play keys use those stems after the mesh
is placed in a scene and clips are auto-filled from Import.

## What this does not verify

- **Play / Edit viewport** idle↔walk blend (Task 5.1; `dogwalk-animation-phase-2`).
- **Near-disk-only** Import of Chocomel without multi-select — idle/walk under
  `animations/world` will **not** attach (by design).
- **Cook** or runtime pose quality on device.

## Automated coverage

Integration test `importExternalFilesPairsCompanionsIntoMeshImport()` in
`engine/src/tests/asset_import_test.cpp` exercises the same flow with synthetic
fixtures:

- `writeSkinnedMeshHostGltfFixture` — Chocomel-shaped host (skins≥1, meshes≥1,
  animations=0)
- `kCompanionLoopGltf` — LOOP-shaped companion (skins=1, meshes=0, animations=1)
- Multi-select `importExternalFiles` → one Mesh Asset + three AnimationClip Assets
  (second companion file carries two internal animations, yielding a suffixed clip)

Run: `asset_import_test.exe` (see `docs/agents/testing.md`).
