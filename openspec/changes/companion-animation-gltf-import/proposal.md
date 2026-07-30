## Why

DogWalk Chocomel ships a skinned mesh glTF with **no** animations and separate LOOP idle/walk glTFs under a disconnected tree (`assets/char/…` vs `animations/world/…`). Today Import only extracts clips from the mesh Intermediate itself, so Chocomel cannot become Mesh + AnimationClip Assets and Phase 1/2 Play acceptance stays blocked. Merging DCC files is not the product path we chose; Import must attach **Companion Animation glTFs**.

## What Changes

- Mesh Import (animations enabled) attaches **Companion Animation glTFs** and registers additional **AnimationClip** Assets under the same mesh stem (still one Mesh + N Clips).
- **Primary path:** multi-select batch — exactly one skinned mesh host; other glTFs that pass acceptance become its companions.
- **Secondary path:** near-disk auto-scan (mesh directory + immediate child dirs of its parent) for co-located packs.
- **Acceptance:** companion has animations and **no meshes** (skins allowed — Chocomel LOOP is skins=1, meshes=0).
- Copy accepted companions under Resources as Intermediate bodies (**not** Mesh Assets); clip Intermediate remains YAML; clip logical names prefer companion file stem.
- Bone-name mismatch: warn and still register. Mesh already embedding animations does not skip companion attach.
- **No** Godot AnimationLibrary. **No** hard-coded `animations/world` walk. **No** fuzzy filename host pairing.

## Capabilities

### New Capabilities
- `companion-animation-import`: Discovery, acceptance, multi-select host pairing, companion Intermediate copy, clip extract/register from companions, Reimport refresh from stored companions.

### Modified Capabilities
- `asset-import`: Mesh Import with animations enabled SHALL invoke companion attach (multi-select and/or near-disk) in addition to in-file clip extract; SHALL NOT register companions as Mesh Assets.

## Impact

- Code: `AssetImportService::importMesh` / `importExternalFiles`, `gltf_animation_clip_extractor`, mesh Reimport refresh paths, Content Browser multi-select Import.
- Docs: CONTEXT **Companion Animation glTF**; [ADR 0021](../../../docs/adr/0021-companion-animation-gltf-import.md).
- Unblocks Chocomel content Import for `dogwalk-animation-phase-1` / `dogwalk-animation-phase-2` Done gates (5.3 etc.) after Test Project multi-select Import.
- Tests: unit/integration for acceptance, multi-select pairing, near-disk scan, clip naming, Reimport from companion Intermediate.
- Non-goals: AnimationLibrary, AnimationTree, merging glTFs offline as the required workflow, DogWalk-hardcoded folder conventions.
