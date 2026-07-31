# Manual checklist — companion animation glTF Import (Tasks 5.1, 5.2)

Human verification only. Automated coverage lives in `asset_import_test` (`importExternalFilesPairsCompanionsIntoMeshImport`, `singleMeshImportDiscoversNearDiskCompanions`). Passing this checklist does **not** mark `dogwalk-animation-phase-1` Done (Chocomel Play acceptance remains a separate gate).

## Prerequisites

| Item | Notes |
|------|-------|
| Build | Debug `engine_editor` from this branch (`cmake --build build/vs2026-debug --config Debug --target engine_editor`) |
| Project | `E:\Blunder Projects\Test` (or another Blunder Project with Content Browser + Import) |
| Source tree | Godot DogWalk repo on disk. Paths below are relative to the repo root (`E:\Godot Projects\dogwalk-repo` on the verification machine). |
| Import target | Content Browser folder `assets/Meshes` selected before Import |
| Mesh dialog | **Import Animations** checked in the Import Mesh dialog |

DogWalk Chocomel layout (disconnected tree — multi-select required):

| Role | Path |
|------|------|
| Host mesh | `pro/game/assets/char/chocomel/Chocomel.gltf` |
| Idle companion | `pro/game/animations/world/LOOP-chocomel-idle/LOOP-chocomel-idle.gltf` |
| Walk companion | `pro/game/animations/world/LOOP-chocomel-walk/LOOP-chocomel-walk.gltf` |

---

## 5.1 — Content Browser multi-select (Chocomel + idle + walk)

**Goal:** Multi-select Import registers one Mesh + companion-derived AnimationClip Assets; clips are addressable by file stem in Edit preview and Play.

### Steps

1. Launch `engine_editor` with the Test Project.
2. Open Content Browser; select folder `assets/Meshes`.
3. **Import Asset** (or OS drag-drop onto Content Browser) and multi-select exactly:
   - `pro/game/assets/char/chocomel/Chocomel.gltf`
   - `pro/game/animations/world/LOOP-chocomel-idle/LOOP-chocomel-idle.gltf`
   - `pro/game/animations/world/LOOP-chocomel-walk/LOOP-chocomel-walk.gltf`
4. In **Import Mesh**, leave **Import Animations** enabled; confirm Import.
5. After Content Browser refresh, inspect Assets:

### Expected — Assets tree

- [ ] Exactly **one** Mesh descriptor: `assets/Meshes/Chocomel.mesh.yaml`
- [ ] **No** Mesh descriptors for the LOOP files (`LOOP-chocomel-idle.mesh.yaml`, `LOOP-chocomel-walk.mesh.yaml` must **not** exist)
- [ ] AnimationClip descriptors under `assets/Animations/` named from companion stems, e.g.:
  - `LOOP-chocomel-idle.animation.yaml`
  - `LOOP-chocomel-walk.animation.yaml`
  - (If a companion glTF embeds multiple animations, expect stem + `_1`, `_2`, … suffixes per integration test convention.)

### Expected — Resources Intermediate

- [ ] Host body under `resources/Models/Chocomel/…`
- [ ] Companion copies under `resources/Models/Chocomel/companions/`:
  - `LOOP-chocomel-idle.gltf`
  - `LOOP-chocomel-walk.gltf`
- [ ] Mesh descriptor lists both paths in `companion_animation_sources`

### Expected — Edit / Play clip addressing

6. Place imported Chocomel in a scene (or open an existing DogWalk acceptance scene once content is wired).
7. Select entity with **AnimationPlayer**; confirm Inspector name→GUID map includes stem keys **`LOOP-chocomel-idle`** and **`LOOP-chocomel-walk`** (auto-filled from Import).
8. **Edit Mode:** use authorship viewport AnimationPlayer controls to preview **`LOOP-chocomel-idle`** then **`LOOP-chocomel-walk`** — skeleton updates without starting Play / Behaviour Tick.
9. **Play Mode (optional smoke):** `Play("LOOP-chocomel-walk")` from Behaviour or equivalent — clip resolves (hard cut / deformation need not pass Phase 1 Done here).

### Pass criteria (5.1)

- [ ] All Expected checks above pass
- [ ] No orphan-companion warning for this batch (host + two companions)
- [ ] Reimport host Mesh preserves clip GUIDs (spot-check: note a clip GUID, Reimport, confirm unchanged)

**Verifier:** _______________ **Date:** _______________

---

## 5.2 — Near-disk vs disconnected discovery

**Goal:** Co-located companion folders attach on **single-file** mesh Import; disconnected DogWalk `animations/world/…` tree does **not** attach without multi-select.

### A — Co-located near-disk pack (single-file Import)

Use a **local test folder** (outside the Project) mirroring the integration-test layout:

```
<co_located_root>/
  assets/char/chocomel/Chocomel.gltf          ← skinned host (DogWalk file or copy)
  assets/char/animations/LOOP-idle.gltf       ← companion (animations present, meshes=0)
  assets/char/animations/LOOP-walk.gltf       ← companion
```

1. Content Browser → `assets/Meshes`; select folder.
2. Import **only** `…/assets/char/chocomel/Chocomel.gltf` (single selection).
3. **Import Animations** enabled; confirm.

**Expected (co-located):**

- [ ] Mesh `Chocomel.mesh.yaml` registered
- [ ] Clips from sibling child folder attached (e.g. `LOOP-idle.animation.yaml`, `LOOP-walk.animation.yaml` — names follow companion file stems)
- [ ] `companion_animation_sources` lists copied companions under `resources/Models/Chocomel/companions/`
- [ ] No clip registered from paths outside near-disk scope (no deep `…/world/deep.gltf`-style files)

### B — Disconnected Chocomel tree (single-file Import — negative)

1. **Delete** or use a fresh Project subfolder so prior Chocomel Import does not mask results.
2. Content Browser → `assets/Meshes`.
3. Import **only** DogWalk `pro/game/assets/char/chocomel/Chocomel.gltf` (do **not** multi-select LOOP files).
4. **Import Animations** enabled; confirm.

**Expected (disconnected — must NOT attach):**

- [ ] Mesh `Chocomel.mesh.yaml` registered
- [ ] **No** `LOOP-chocomel-idle` / `LOOP-chocomel-walk` AnimationClip Assets from near-disk discovery alone
- [ ] `companion_animation_sources` empty (or absent) on the Mesh descriptor
- [ ] Log may note zero companions; Import must **not** fail the host Mesh

**Recovery:** Run §5.1 multi-select Import to attach companions for real content work.

### Pass criteria (5.2)

- [ ] §A co-located single-file Import attaches expected companions
- [ ] §B disconnected single-file Import does **not** falsely attach `animations/world/…` LOOP files

**Verifier:** _______________ **Date:** _______________

---

## Out of scope for this checklist

- DogWalk Phase 1/2 **Done** (idle↔walk feel, stepped facing, real-time move) — see `dogwalk-animation-phase-1` task 6.4
- Bone-mismatch warning text review (warn+register is sufficient)
- Reimport from changed **external** companion files (Reimport refreshes persisted Intermediates only)

## References

- [design.md](design.md) — multi-select primary, near-disk secondary
- [specs/companion-animation-import/spec.md](specs/companion-animation-import/spec.md) — Chocomel scenarios
- [docs/adr/0021-companion-animation-gltf-import.md](../../../docs/adr/0021-companion-animation-gltf-import.md)
- [CONTENT_LAYOUT.md](../../../CONTENT_LAYOUT.md) — Content Browser Import entry points
