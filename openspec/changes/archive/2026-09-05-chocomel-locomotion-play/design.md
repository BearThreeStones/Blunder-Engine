## Context

See proposal.md for why. Engine AnimationTree already round-trips scene-embedded BlendSpace1D (`applyAnimationTreeTopology` in `scene_instance.cpp`; fixture in `scene_behaviour_instantiate_test.cpp`). C# can Travel / Start / SetBlendSpaceScalar; it cannot add BlendSpace points. Clip maps still serialize under `animationPlayer.clips` while the product term is Clip Binding on the tree. Skeleton bones are not in the scene file; `hydrateSkeletonFromEntityMesh` runs only on Inspector Add… Skeleton (ADR 0034). Test content lives in `E:\Blunder Projects\Test` (outside this repo’s default edit root).

World is Z-up, +X right, +Y forward. Mesh forward is +Y. Stored rotation is Quat; Play scripts write TRS through Object.

## Goals / Non-Goals

**Goals:**

- New Test Behaviour that only talks to an active AnimationTree + Object TRS + PoseApplied facing.
- Topology JSON that matches the engine fixture names (`Locomotion` / `idle` / `walk`).
- Two-step content landing: script first, topology patch after the author saves the editor-assembled scene.

**Non-Goals:**

- Engine hydrate-on-instantiate, Tree Asset, AnimationTree Canvas, C-ABI changes.
- Rewriting `PlayerMove`, changing startup scene, Clip Play for locomotion, fake Add2/OneShot.
- Follow camera, physics, Jump, Pinda/leash.

## Decisions

1. **New Behaviour, not a `PlayerMove` dual-path**  
   `character_move` must keep two-slot proof. Dual-path in one class is the acceptance shortcut already rejected.  
   *Alternatives:* rewrite `PlayerMove` tree-only (breaks cube scene); runtime branch on `AnimationTree.Active`.

2. **Hardcoded `Locomotion` / no IdleClip fields**  
   BlendSpace points are scene-authored. Behaviour clip-name fields cannot retarget points (no C# `addBlendSpacePoint`) and would imply the script owns clip pick. Inspector knobs: Speed, FrameTime, FacingSlices, TimeScale only.  
   *Alternatives:* `[BehaviourClipName]` Idle/Walk (misleading); exposable node-name strings (YAGNI for this fixture).

3. **PoseApplied and TimeScale stay on AnimationPlayer**  
   Active tree notifies Player PoseApplied and mirrors dominant-clip `PlaybackPosition`. Product TimeScale is still the Player scheduler field. Script: `EnsureAnimationTree` + `EnsureAnimationPlayer` for clock/listener/TimeScale only — no SetSlot/Play.  
   *Alternatives:* new AnimationTree.PoseApplied event (engine scope); skip facing this slice.

4. **Reuse `PlayerMoveMath` and `SteppedFacing`**  
   Same stick→[0,1] scalar and +Y→0° yaw as the cube path so facing tests stay valid.  
   *Alternatives:* duplicate math on the new type.

5. **Inactive tree: log and skip, never auto-activate**  
   Matches Clip Play / product “tree must already be active.” Author leaves `animationTree.active: true` in the document.  
   *Alternatives:* `Active = true` in Ready (hides authoring mistakes).

6. **Author hydrates Skeleton in-session; agent patches JSON after save**  
   Instantiating hand-written `hasSkeleton` yields empty bones. Grilling locked no engine hydrate. Sequence: land C# → author Add Skeleton / Add Tree / bind clips / attach Behaviour → patch `animationTree` block. Same editor session Play is the acceptance path; reload may lose bones until a later hydrate-on-load change.  
   *Alternatives:* hydrate on instantiate (engine); defer mesh (rig-only, no visible dog).

7. **Empty `assetGuid`; omit add2 / oneShotClip keys**  
   Non-empty Tree Asset GUID makes instantiate apply overrides only (no points). Empty Add2/OneShot avoids binding walk as turn.  
   *Alternatives:* Tree Asset (Phase 5); stub turn with walk (rejected).

8. **Clip Binding logical names `idle` / `walk`**  
   Must match BlendSpace point names. Assets: `LOOP-chocomel-idle` / `LOOP-chocomel-walk` (not `_1`). Disk GUIDs stay in the serialized clip map; authors pick clips in Inspector.

## Risks / Trade-offs

- [Reload empty Skeleton] → Document in-session Play; do not treat T-pose after reopen as this slice’s engine bug. Follow-up: hydrate on instantiate.
- [Author scene not saved / wrong names] → Topology patch waits for the file; bind names MUST be `idle`/`walk` or Travel samples nothing.
- [EnsureAnimationTree on an entity without tree] → Creates empty tree; Ready Start fails. Author must Add… AnimationTree before Play.
- [Scripts.Tests won’t cover the new Behaviour] → Accepted; existing math/facing tests plus manual Play.

## Migration Plan

1. Add `ChocomelLocomotion.cs`; build Test Scripts; keep `PlayerMove` scenes working.
2. Author assembles and saves `chocomel_locomotion.scene.asset`.
3. Patch embedded `animationTree` topology; author Play in the same session.
4. Rollback: delete the new script + scene; no engine revert.

## Open Questions

None. Grilling locked names, split script, hydrate ownership, and Done bar.
