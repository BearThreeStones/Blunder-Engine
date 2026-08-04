## Why

Phase 4 delivers a lean AnimationTree (StateMachine, BlendSpace1D, OneShot, Add2), but DogWalk still needs **post-pose procedural bones**, **clip method events**, **BlendSpace2D** (Pinda-style), and **reusable Tree authorship** without full Godot parity or a cutscene director. Grilling locked **DogWalk animation Phase 5** as three independent Done gates under [ADR 0026](../../../docs/adr/0026-animation-phase-5.md).

## What Changes

- **Gate A — SkeletonModifier + method tracks**
  - Ordered **SkeletonModifier** ClassDB chain after Player/Tree sample and **before** PoseApplied (extension point + test double + one minimal LookAt/aim sample).
  - **Method tracks** in AnimationClip Intermediate YAML; engine dispatches on **key-crossing** using the **base dominant-clip clock** (OneShot clock while active) to co-located Behaviours / optional Message — not engine PtrCall of arbitrary methods.
- **Gate C — BlendSpace2D**
  - AnimationTree node: 2D parameter plane; **triangulation + barycentric** TRS lerp/slerp; per-node `(x,y)` named API.
- **Gate D — AnimationTree Asset (D-core)**
  - First-class **AnimationTree Asset** (GUID); Inspector topology authorship; **Asset as base** with **small scene instance overrides** when referenced; Phase 4 embed-only remains when no Asset. Visual node canvas is **optional** and does **not** block Done.
- **Edit Mode**: scrub modifiers / BlendSpace2D / Asset+overrides without DotNetHost / Behaviour Tick.
- C-ABI / Blunder.Api for new surfaces.
- **Done**: all three engineering gates **plus** lean Play bars per gate. Suggested implement order **A → C → D**. May proceed in parallel with unfinished Phase 1–4 content gates (those Done criteria are not cancelled).

**Out of scope:** audio tracks as mandatory; Cubic/Bezier; cross-Object Skeleton drive; AnimationLibrary; full cutscene director; visual AnimationTree canvas as Done; full Pinda/leash/paper-mouth content parity as the only Play bar; cancelling Phase 1–4 content Done gates.

## Capabilities

### New Capabilities
- `skeleton-modifier`: post-pose ordered modifier chain before PoseApplied
- `animation-method-track`: clip YAML method keys + dominant-clock key-crossing dispatch
- `blend-space-2d`: BlendSpace2D triangulation/barycentric node + named `(x,y)` API
- `animation-tree-asset`: GUID Tree Asset + Inspector authorship + Asset-base/scene-override rules

### Modified Capabilities
- `animation-tree`: hosts BlendSpace2D; may reference AnimationTree Asset; sample stack still base→Add2 then modifiers
- `dogwalk-animation-content`: Phase 5 per-gate lean Play bars; parallel with open Phase 1–4 content gates
- `engine-c-abi` / `script-native-abi`: SkeletonModifier, method-track hooks, BlendSpace2D, Tree Asset entry points

## Impact

- Engine: modifier pipeline, clip method data + dispatch, BlendSpace2D sampler, Tree Asset I/O + Inspector, Edit scrub, C-ABI + Blunder.Api
- Specs: four new capabilities; deltas on animation-tree, dogwalk-animation-content, ABI
- Content: harnesses + lean Play per gate; earlier phase content bars stay tracked
- Docs: CONTEXT Phase 5 terms; ADR 0026
- Depends on: Phase 4 AnimationTree foundation (branch from phase-4 tip recommended)
