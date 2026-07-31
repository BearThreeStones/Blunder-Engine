## Why

Phase 1 delivers hard-cut AnimationClip playback and Stepped hooks, but DogWalk locomotion needs **continuous idle↔walk weighting**, **soft clip switches**, and **playback rate** — without jumping to a full AnimationTree. Grilling locked **DogWalk animation Phase 2** (P2 only: unified two-slot blend + global TimeScale) under [ADR 0020](../../../docs/adr/0020-animation-player-two-slot-blend.md).

## What Changes

- Extend **AnimationPlayer** to a **unified two-slot** model: explicit slot clip assignment, single `blendWeight ∈ [0,1]`, local TRS lerp / rotation slerp pose combine ([ADR 0020](../../../docs/adr/0020-animation-player-two-slot-blend.md)).
- **Crossfade** = time-driven weight ramp on those same two slots (`Play(name, fade)` sugar). **Hard cut** remains (`fade = 0`).
- Add **global TimeScale** on the AnimationPlayer (scales all active slot advance; no per-track TimeScale in Phase 2).
- Expose **dominant-slot** playback position for Animation step / PoseApplied consumers under blend.
- Persist scene **defaults**: TimeScale + optional default slot clip names + initial blend weight (in addition to name→GUID map). Not a full live playback snapshot.
- **Edit Mode** authorship scrub: TimeScale, fade, two-slot weights without DotNetHost / Behaviour Tick. Stepped facing stays Play-validated.
- **C-ABI / Blunder.Api** bindings for slots, blendWeight, TimeScale, `Play` with fade.
- Content: engineering gate (test-rig or equivalent) + **Chocomel (or agreed subset) Play** with weighted idle↔walk, perceptible TimeScale, stepped facing on dominant slot. May proceed **in parallel** with unfinished Phase 1 Chocomel hard-cut acceptance (Phase 1 Done criteria not cancelled).

**Out of scope:** AnimationTree / state machine / BlendSpace graphs; additive/Add2 layers; per-track TimeScale; N-way blend; method/audio tracks; procedural bone modifiers; SYNC/CINE; Cubic/Bezier; in-editor Behaviour Tick; requiring Edit to match Play Stepped feel as Done.

## Capabilities

### New Capabilities
<!-- Phase 2 extends Phase 1 animation capabilities; no separate product domain. -->

### Modified Capabilities
- `skeletal-animation`: (baseline from `dogwalk-animation-phase-1`, not yet archived into `openspec/specs/`) — two-slot blend, Crossfade sugar, hard-cut fade=0, global TimeScale, dominant-slot clock, scene defaults, Edit scrub for blend/TimeScale
- `dogwalk-animation-content`: Phase 2 weighted idle↔walk + TimeScale Play acceptance; parallel with Phase 1 hard-cut Chocomel gate
- `engine-c-abi`: C-ABI entry points for two-slot / blendWeight / TimeScale / Play-with-fade
- `script-native-abi`: Blunder.Api managed surface matching the C-ABI (NativeAbi table completeness)

## Impact

- Engine: AnimationPlayer sampler/combine, advance under TimeScale, scene serialize defaults, Edit preview toolbar, C-ABI + Blunder.Api
- Specs: deltas on skeletal-animation + dogwalk-animation-content (Phase 1 change) and engine-c-abi / script-native-abi (main)
- Content: Test Project scripts drive `SetSlot` + blendWeight; Chocomel (or subset) weighted Play
- Docs: CONTEXT Phase 2 terms already grilled; ADR 0020 accepted
- Depends on: Phase 1 AnimationPlayer / PoseApplied / skinning foundation (may implement Phase 2 engine while Phase 1 Chocomel hard-cut remains open)
