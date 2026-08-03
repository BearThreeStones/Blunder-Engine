## Why

Phase 3 delivers Sync Group + CINE, but DogWalk locomotion still needs **BlendSpace1D speed blend**, **StateMachine / OneShot**, and **Add2** overlays without shipping full Godot AnimationTree parity or procedural bone modifiers. Grilling locked **DogWalk animation Phase 4** as a lean **AnimationTree** under [ADR 0025](../../../docs/adr/0025-animation-tree-phase-4.md).

## What Changes

- Add **AnimationTree**: co-located ClassDB playback graph on the same Object as AnimationPlayer + Skeleton; resolves clips via the player's name→GUID map.
- When Tree is **active**, it **exclusively** samples the Skeleton; AnimationPlayer Play / two-slot / Crossfade do not write bones until inactive (Phase 1–3 path resumes).
- Lean node set: **StateMachine** (travel/start), **BlendSpace1D**, **OneShot**, **Add2** — sample order **base then additive**; Add2 deltas relative to **bind/rest**.
- Scripts drive via a **narrow named API** (per-node BlendSpace scalars by node logical name) — not Godot `parameters/…` paths as the primary surface.
- **Graph topology is scene-embedded** on AnimationTree; Phase 4 does **not** require a visual node editor or standalone AnimationTree Asset.
- Sync Group **Fire**: no active tree → Phase 3 hard-cut Play; **active tree → OneShot** (does not deactivate the tree by default).
- PoseApplied retained; Animation step uses **base dominant clip** clock; AnimationPlayer **TimeScale** scales the whole active tree.
- **Edit Mode**: activate tree + scrub named drives without DotNetHost / Behaviour Tick; Stepped feel remains Play-validated.
- C-ABI / Blunder.Api for AnimationTree + updated Sync Fire→OneShot behaviour.
- **Done**: engineering gate + Chocomel (or agreed subset) Play acceptance (perceptible BlendSpace motion, visible additive turn, OneShot return-to-base, stepped facing on base dominant clock). May proceed **in parallel** with unfinished Phase 1–3 content gates (those Done criteria are not cancelled).

**Out of scope:** BlendSpace2D / Pinda-complete graphs; procedural bone modifiers; method/audio tracks; Cubic/Bezier; cross-Object Skeleton drive; AnimationLibrary; full cutscene director; visual AnimationTree editor / standalone Tree Asset as Done; Godot parameter-path primary API; per-node TimeScale; Fire→travel as the only Sync path; cancelling Phase 1–3 content Done gates.

## Capabilities

### New Capabilities
- `animation-tree`: AnimationTree runtime graph (StateMachine, BlendSpace1D, OneShot, Add2), exclusive sampling when active, scene-embedded topology, narrow named script API, Edit scrub

### Modified Capabilities
- `skeletal-animation`: active AnimationTree owns Skeleton sampling; PoseApplied / dominant-clip step clock under tree; TimeScale applies globally to tree advance
- `animation-sync-cine`: Sync Group Fire on active-tree members applies as OneShot
- `dogwalk-animation-content`: Phase 4 Chocomel-subset Play acceptance on AnimationTree; parallel with open Phase 1–3 content gates
- `engine-c-abi` / `script-native-abi`: AnimationTree entry points; Fire→OneShot semantics when tree active

## Impact

- Engine: AnimationTree ClassDB type, graph sample stack, scene serialization, Edit preview, C-ABI + Blunder.Api
- Specs: new `animation-tree`; deltas on skeletal-animation, animation-sync-cine, dogwalk-animation-content, ABI surfaces
- Content: Test Project / harness for BlendSpace+Add2+OneShot; Phase 1–3 content bars stay tracked separately
- Docs: CONTEXT Phase 4 terms grilled; ADR 0025 accepted
- Depends on: Phase 1–3 AnimationPlayer / Sync Group foundation (may implement Phase 4 engine while earlier Chocomel/SYNC content gates remain open)
