## Context

DogWalk Move slice proved Play + C# Behaviour without animation. Grilling locked **DogWalk animation Phase 1** (P0 skeletal playback + P1 Stepped feel via C#) and **[ADR 0019](../../../docs/adr/0019-gltf-intermediate.md)** (glTF Intermediate; COLLADA removed). Engine today: static MeshRenderer only; asset specs still mandate COLLADA Intermediate (ADR 0013 era). Stakeholders: engine runtime/render, asset pipeline, DogWalk content authors on Blender→glTF.

## Goals / Non-Goals

**Goals:**

- glTF/GLB mesh Intermediate (+ AnimationClip YAML); remove COLLADA path
- Skeleton + AnimationPlayer on same Object; clip Play by name; PoseApplied; Tick → sample → PoseApplied
- Fast Path CPU skin / Final GPU skin
- Edit Mode AnimationPlayer preview (no Behaviour)
- Content: test-rig gate + Chocomel Play acceptance (idle↔walk, real-time move, stepped facing)

**Non-Goals:**

- AnimationTree / BlendSpace / StateMachine / Add2 / OneShot / TimeScale
- method/audio tracks; procedural SkeletonModifier
- SYNC/CINE; Cubic/Bezier
- In-editor Behaviour Tick for preview
- Silent `.blend` export
- glTF as Final

## Decisions

1. **Mesh Intermediate = glTF (ADR 0019)**  
   Import glTF/GLB as Intermediate (copy under Resources). FBX/OBJ Source-Export → glTF when supported. `.blend` remains Source-only / reject auto-export.  
   *Alternatives:* COLLADA+YAML (rejected — skin/Stepped risk); dual Intermediate (rejected debt).

2. **AnimationClip = YAML sidecar Asset**  
   Import extracts each glTF animation to readable YAML (bone TRS, time, Constant|Linear). Cook → Final clip binary.  
   *Alternatives:* clips only inside glTF (rejected — no first-class Clip GUID / Play map); binary-only Intermediate (rejected — hard to diff/debug Stepped).

3. **Import yields 1 Mesh + N Clips**  
   Shared skin/bind on Mesh; N AnimationClip GUIDs. AnimationPlayer name→GUID auto-filled, Inspector-editable.  
   *Alternatives:* single bundled Asset (rejected).

4. **Object co-located Skeleton + AnimationPlayer**  
   ClassDB types on same Object; Phase 1 no cross-Object skeleton drive. Skinned MeshRenderer binds to that Skeleton.  
   *Alternatives:* pure ECS-first (deferred); Godot-style child Objects (rejected for Phase 1).

5. **Frame order: Tick → sample → PoseApplied**  
   Behaviours Play/move in Tick; visuals that need final pose use PoseApplied (C# frametime modulo for Animation step).  
   *Alternatives:* sample-before-Tick (late Play); engine builtin AnimationStep (DogWalk policy in engine — rejected).

6. **Skinning split by path**  
   Fast Path CPU; Final GPU. Editor with fresh Final matches Player.  
   *Alternatives:* editor-always-CPU (pixel drift); GPU-only Phase 1 (blocks Intermediate preview).

7. **Edit preview = AnimationPlayer only**  
   Authorship viewport Play/Pause/Stop/Loop clips; no DotNetHost for preview.  
   *Alternatives:* full in-editor Behaviour (conflicts Play-in-Player ADR).

8. **ValueSlicer in C#**  
   Project utility/Behaviour; not ClassDB.  
   *Alternatives:* engine ValueSlicer (rejected — art direction in engine).

9. **cgltf (or existing glTF stack) primary Intermediate reader**  
   Prefer restoring/keeping glTF reader for Intermediate; Assimp remains for FBX/OBJ→glTF export if needed. Retire Assimp COLLADA as Intermediate authority.  
   *Alternatives:* Assimp-read-everything (heavier; less control of STEP keys).

## Risks / Trade-offs

- [COLLADA content already upgraded] → Migration: GUID-preserving dae→glTF or Reimport from Source; fail-soft log
- [Chocomel import noise (extra bones/tracks)] → Subset clips / strip unused; test-rig unblocks engine CI feel
- [CPU vs GPU skin visual parity] → Same bind/pose math; golden pose compare test on one frame
- [PoseApplied reentrancy] → Document: no Play from inside PoseApplied that re-enters same player mid-callback (or queue)
- [Large YAML clips] → Accept Phase 1; optional cook-time key dedupe later (Godot-style optimizer)
- [Edit preview vs Play feel diverge] → Explicit: Stepped facing only Play-validated

## Migration Plan

1. Land ADR 0019 + OpenSpec deltas; implement glTF Intermediate Import/Cook/Fast Path; remove dae Intermediate-direct
2. Migration pass: `.dae` Intermediate → glTF (or Reimport); delete upgrade-to-dae
3. Skeleton/AnimationPlayer + CPU skin Fast Path; Edit preview
4. GPU skin Final + Player parity
5. Clip YAML extract + name map; C# ValueSlicer + step sync
6. Test-rig then Chocomel acceptance
7. Rollback: revert change; projects on glTF Intermediate remain valid; dae-only trees need Reimport from Source

## Open Questions

- Exact AnimationClip YAML schema field names (apply-time; must round-trip STEP/LINEAR)
- FBX/OBJ→glTF export tool (Assimp vs external) if required in Phase 1 — defer if content is glTF-only
- GPU bone palette / max joints limit — set during GPU skin spike
