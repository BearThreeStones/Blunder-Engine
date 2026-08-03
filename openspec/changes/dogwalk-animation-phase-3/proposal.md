## Why

Phase 2 delivers weighted dual-track blend and TimeScale on a single AnimationPlayer, but DogWalk still needs **multi-Object coordinated starts** (character + prop + partner) and **short cinematic handoffs** without shipping an AnimationTree or cutscene director. Grilling locked **DogWalk animation Phase 3** as **SYNC + CINE** under [ADR 0023](../../../docs/adr/0023-animation-sync-group.md).

## What Changes

- Add **Sync Group**: runtime engine set of AnimationPlayer members; create / join / fire / release by script (not a required serialized scene member graph).
- **Fire** carries **per-member** `(AnimationPlayer, clipName[, seek])` instructions; heterogeneous clip names are first-class; same-name Fire MAY be sugar only.
- **Default Fire is hard cut** (`Play` fade 0 / equivalent); Crossfade is not the Sync Group default.
- Add **CINE** segment contract: Enter / explicit **End** API; optional **in-CINE** marking and gameplay-input suppression; pose alignment and gameplay state remain in C# Behaviours.
- **Edit Mode**: preview Sync Group Fire + CINE Enter/End (visible in-CINE / suppression marks, multi-Skeleton playback) **without** DotNetHost / Behaviour Tick and **without** automatic Object TRS / gameplay state-machine handoff.
- C-ABI / Blunder.Api bindings for Sync Group + CINE hooks.
- **Done**: engineering gate + DogWalk-style mini Play acceptance (character + prop/partner sync start + CINE handoff returning control; simplified props OK). May proceed **in parallel** with unfinished Phase 2 Chocomel weighted acceptance (Phase 2 Done not cancelled).

**Out of scope:** AnimationTree / state machine / BlendSpace; additive/Add2; procedural bone modifiers; full Cutscene Director / multi-track timeline; shared continuous playback head across Sync Group members; requiring identical clip names for all members; Edit auto-snap TRS; in-editor Behaviour Tick as CINE preview driver; Cubic/Bezier; method/audio tracks as Phase 3 requirement.

## Capabilities

### New Capabilities
- `animation-sync-cine`: Sync Group runtime coordination + CINE segment enter/end contract (Edit preview of marks + multi-Object Fire; Play handoff in C#)

### Modified Capabilities
- `skeletal-animation`: AnimationPlayer remains co-located per Object; Phase 3 does not reopen cross-Object Skeleton drive — Sync Group coordinates multiple Players, each driving its own Skeleton
- `dogwalk-animation-content`: Phase 3 mini multi-Object SYNC/CINE Play acceptance; parallel with Phase 2 weighted Chocomel gate
- `engine-c-abi` / `script-native-abi`: Sync Group + CINE entry points

## Impact

- Engine: Sync Group service, CINE session hooks, Edit preview affordances, C-ABI + Blunder.Api
- Specs: new `animation-sync-cine`; deltas on skeletal-animation, dogwalk-animation-content, ABI surfaces
- Content: Test Project mini scene (character + simplified prop/partner); Phase 2 Chocomel weighted bar stays tracked separately
- Docs: CONTEXT Phase 3 terms grilled; ADR 0023 accepted
- Depends on: Phase 1/2 AnimationPlayer foundation (may implement Phase 3 engine while Phase 2 Chocomel weighted acceptance remains open)
