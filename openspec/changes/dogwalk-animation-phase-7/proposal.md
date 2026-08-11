## Why

Phase 5 D shipped **AnimationTree Asset** + Inspector and left the visual canvas optional; Phase 4–6 explicitly excluded canvas-as-Done. Authors still hand-edit topology or fight Inspector lists, and scripts must `Travel` for every state change. Grilling locked **DogWalk animation Phase 7** under [ADR 0030](../../../docs/adr/0030-animation-tree-canvas-phase-7.md): **AnimationTree Canvas** (Asset truth) plus **parameter-driven StateMachine transitions**.

## What Changes

- **AnimationTree Canvas** v1: visual node graph editing the **AnimationTree Asset** body (not scene-embed as truth source).
- Dual-track authorship with **Inspector**; canvas **layout** persisted in the Asset body.
- Open from **Content Browser** (Asset) and **Object Inspector** (referenced Asset).
- Topology-only Edit path for Done; bound live preview character is a follow-up.
- Canvas covers Phase 5 nodes: **StateMachine**, **BlendSpace1D**, **BlendSpace2D**, **OneShot**, **Add2**.
- **StateMachine transitions**: real edges with hybrid condition inputs, single predicate per edge, author priority, hard-cut auto Travel; **Travel/Start coexist**.
- Lean **C-ABI / Blunder.Api** for tree parameters and any transition-related queries/setters needed for Edit/Play.
- **Done**: engineering + Edit authorship + lean Play bar (perceptible auto state change via conditions). Phase 1–6 open content gates stay tracked separately.

**Out of scope:** Godot-complete AnimationTree; canvas editing scene-embed as product truth; removing Inspector topology; bound canvas preview / in-canvas mini viewport as Done; clip-end-only as sole transition trigger; per-edge fade/Crossfade; AND/OR or free expressions on one edge; closing Phase 1–6 content Done via Phase 7 alone.

## Capabilities

### New Capabilities
- `animation-tree-canvas`: Visual graph editor for AnimationTree Asset topology + layout; dual open paths; dual-track with Inspector
- `state-machine-transition`: Conditional StateMachine edges (hybrid inputs, single predicate, priority, hard-cut, Travel coexist)
- `dogwalk-animation-content`: Phase 7 engineering/Edit + lean Play Done; parallel with open Phase 1–6 gates

### Modified Capabilities
- `animation-tree`: Hosts transition evaluation + independent tree parameters alongside existing named drives
- `animation-tree-asset`: Asset body stores transitions, tree parameters, and canvas layout with topology
- `engine-c-abi`: Entry points for tree params / transition-related controls as needed
- `script-native-abi`: Blunder.Api façades + NativeAbi completeness for those entry points

## Impact

- Engine: StateMachine transition runtime; tree parameter table; Asset serialize layout + edges; Canvas UI (Slint/editor); Inspector remains writable
- Specs: three new capabilities; deltas on animation-tree, animation-tree-asset, ABI
- Content: harness + lean Play for condition-driven auto Travel; earlier phase content bars stay tracked
- Docs: CONTEXT Phase 7 / Canvas / transition terms; ADR 0030
- Depends on: Phase 5 AnimationTree Asset + Phase 4–5 tree node set (main tip / phase worktrees as applicable)
