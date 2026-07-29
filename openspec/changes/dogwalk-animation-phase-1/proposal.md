## Why

The Move-only **DogWalk character slice** proved Play + C# Behaviour, but characters still cannot look like DogWalk: there is no skeletal AnimationClip runtime, no Stepped-feel hooks, and mesh Intermediate is still specified as COLLADA — which fights Blender’s glTF-first DogWalk authoring and risks skin/Stepped keys. Without **DogWalk animation Phase 1**, further DogWalk content stays blocked on a missing engine foundation.

## What Changes

- **BREAKING:** Remove COLLADA (`.dae`) as mesh Intermediate; mesh Intermediate is **glTF/GLB** again ([ADR 0019](../../../docs/adr/0019-gltf-intermediate.md) supersedes ADR 0013). Retire glTF→COLLADA Intermediate Upgrade; migrate remaining `.dae` GUID-preserving to glTF or reimport from Source.
- Add **AnimationClip** Assets (readable YAML Intermediate of skeletal TRS keys; Constant + Linear) extracted on Import from multi-animation glTF (**1 Mesh Asset + N Clip Assets**).
- Add engine **Skeleton** + **AnimationPlayer** on the same Object: Play/Stop/Loop by logical clip name (name→GUID map, Import auto-fill + Inspector editable); hard cuts only; PoseApplied after sample; per-frame order Tick → sample → PoseApplied.
- **Skinning:** Fast Path (Intermediate) = CPU; Final (Cooked) = GPU — Editor after Cook and Player share Final/GPU.
- **Edit Mode** may preview AnimationPlayer only (no .NET / Behaviour Tick). Stepped feel (ValueSlicer + Animation step sync) is C# under Play.
- Content acceptance: minimal skinned test-rig gate **and** Chocomel (or agreed subset) Play hard criteria — idle↔walk, real-time move, stepped facing.

**Out of scope (later phases):** BlendSpace / TimeScale / StateMachine / Add2 / OneShot; method/audio tracks; procedural bone modifiers; SYNC/CINE; Cubic/Bezier; in-editor Behaviour Tick for preview; AnimationTree editor.

## Capabilities

### New Capabilities
- `skeletal-animation`: Skeleton + AnimationPlayer ClassDB surface, AnimationClip playback (Constant/Linear), PoseApplied, Edit Mode clip preview, scene co-location rules
- `animation-skinning`: CPU skinning on Fast Path; GPU skinning on Final; skinned mesh draw path
- `dogwalk-animation-content`: Phase 1 content contract — ValueSlicer + step sync Behaviours/utilities, test rig, Chocomel Play acceptance

### Modified Capabilities
- `asset-identity`: mesh Intermediate body = glTF/GLB; COLLADA removed; AnimationClip Intermediate = YAML sidecar
- `asset-import`: glTF/GLB Intermediate-direct (or copy); Source Export target glTF not COLLADA; multi-anim → N Clip Assets; clip YAML extract
- `asset-pull-cook`: Fast Path/Cook consume glTF Intermediate (+ clip YAML); retire COLLADA upgrade; dependency edges for AnimationClip / skinned mesh

## Impact

- Engine: Import/Cook/Fast Path readers; ClassDB Skeleton/AnimationPlayer; render skinning (CPU + GPU); C-ABI / Blunder.Api bindings for Play and PoseApplied
- Specs: replace COLLADA Intermediate requirements in asset-* main specs via deltas
- Content: Test Project (or DogWalk Project) — skinned glTF, clips, scene with AnimationPlayer + Move/facing scripts
- Docs: CONTEXT animation terms (grilled); ADR 0019 already written; CONTENT_LAYOUT / OpenSpec main specs update on archive
- Depends on: Play Mode, gameplay input, Behaviour scene, existing Pull pipeline (ADR 0012) with Intermediate format flipped by 0019
