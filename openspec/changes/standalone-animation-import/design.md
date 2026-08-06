## Context

ADR 0021 made multi-select (host + companions) the primary path and near-disk scan secondary. Orphan companions (no single skinned host in the batch) warn and skip. That blocks DogWalk authors who must pick files from different folders one at a time.

Clip extraction already exists (`extractAndRegisterAnimationClipsFromGltf`). AnimationPlayer name→GUID binding remains scene/Inspector-owned.

## Goals / Non-Goals

**Goals:**
- Single-file or multi-file Import of companion-only glTF registers AnimationClip Assets (stem naming).
- Host+companion multi-select and near-disk Mesh Import behavior unchanged.
- Clear logging when a companion is imported standalone vs attached to a host.

**Non-Goals:**
- Auto-updating Mesh `companion_animation_sources` when importing clips later.
- Auto-filling AnimationPlayer clip maps on Import.
- AnimationLibrary / Godot library Asset type.
- Hard-coded `animations/world` discovery walks.
- Changing companion acceptance (still animations ∧ meshes=0, skins allowed).

## Decisions

1. **Orphan = standalone clip Import** — When `mesh_settings.animations` is true, each `orphan_companion_paths` entry is extracted via `extractAndRegisterAnimationClipsFromGltf` with `mesh_stem` and `preferred_clip_stem` derived from the companion file stem (folder under `resources/Animations/{stem}/`).
2. **No synthetic Mesh** — Do not create a Mesh descriptor for companion-only files.
3. **Sidecar copy** — Persist companion Intermediate glTF (+ `.bin` sidecars) under a standalone Intermediate location (e.g. `resources/Models/_standalone_companions/{stem}/` or only clip YAML if extractor does not need the glTF body after extract). Prefer copying Intermediate body so Reimport/refresh can re-extract later; keep lean: extract from the user-selected absolute path first, then optional copy under Resources for durability.
4. **Pairing unchanged for hosts** — Exactly one skinned host still receives companions; multi-host orphans remain orphans (now clip-imported, not silently attached to a wrong host).
5. **Docs** — Update companion checklist / ADR note: standalone Import is supported; multi-select remains the preferred host+clip Reimport pairing path.

## Risks / Trade-offs

- Authors may Import clips without ever attaching to a Mesh for Reimport pairing — acceptable; Player map is still manual.
- Stem collisions with existing `assets/Animations/{stem}.animation.yaml` — reuse existing unique-name helper.
- Changing prior tests that asserted orphans produce no clips — update intentionally.
