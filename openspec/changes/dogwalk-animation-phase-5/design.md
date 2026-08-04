## Context

Phase 4 locked lean AnimationTree ([ADR 0025](../../../docs/adr/0025-animation-tree-phase-4.md)). DogWalk still needs post-pose procedural bones, method events, BlendSpace2D, and reusable tree assets. Grilling locked **DogWalk animation Phase 5** ([ADR 0026](../../../docs/adr/0026-animation-phase-5.md); CONTEXT SkeletonModifier / Method track / BlendSpace2D / AnimationTree Asset / Phase 5). Stakeholders: runtime animation, Import/clip YAML, asset pipeline, Inspector, C-ABI / Blunder.Api, Test Project content. Phase 1–4 content Done gates may still be open; Phase 5 engine work may proceed in parallel.

## Goals / Non-Goals

**Goals:**

- Three independent engineering gates (A, C, D) + lean Play bars; Done = all three
- SkeletonModifier chain before PoseApplied; method-track dispatch on dominant clock
- BlendSpace2D triangulation + barycentric; named `(x,y)` API
- AnimationTree Asset + Inspector; Asset base + small scene overrides
- Edit scrub without Behaviour Tick
- Implement order A → C → D

**Non-Goals:**

- Audio tracks as mandatory; visual canvas as Done
- Cubic/Bezier; cross-Object Skeleton drive; AnimationLibrary; Cutscene Director
- Full Pinda / leash / paper-mouth content parity as sole bar
- Cancelling Phase 1–4 content Done criteria

## Decisions

1. **One milestone, three gates (ADR 0026)**  
   All required for Phase 5 Done; order A→C→D.  
   *Alternatives:* 5a/5b/5c split; single blocking bar.

2. **SkeletonModifier after sample, before PoseApplied**  
   Ordered ClassDB chain; extension + test double + LookAt/aim sample.  
   *Alternatives:* PoseApplied-only C# hacks; tree-only FinalModify node.

3. **Method tracks = YAML + key-crossing dispatch**  
   Dominant-clip clock; Behaviours/Message; not PtrCall.  
   *Alternatives:* script-only position scans; engine PtrCall.

4. **BlendSpace2D = triangulation + barycentric**  
   Per-node `(x,y)` named API.  
   *Alternatives:* grid bilinear; nearest-three distance weights.

5. **D-core = Tree Asset + Inspector**  
   Visual canvas optional, not blocking.  
   *Alternatives:* canvas-required; embed-only forever.

6. **Asset base + small scene overrides**  
   No Asset → Phase 4 embed.  
   *Alternatives:* mutual exclusion; embed-always-wins copy-on-drop.

7. **Done = engineering + lean Play per gate; Edit scrub A/C/D**  
   Full Pinda not required.  
   *Alternatives:* engineering-only; full content parity.

## Risks / Trade-offs

- [Modifier vs Add2 confusion] → Docs + tests: Add2 in-tree additive; modifier post-pose
- [method flood / reentrancy] → Document dispatch timing; snapshot Behaviour list like Message
- [2D triangulation degenerate points] → Apply-time tie-break / fallback; harness coverage
- [Asset override field creep] → Keep override set small; document allowlist at apply-time
- [Phase 1–4 content still open] → Track separately; do not rewrite earlier Done
- [Inspector-only D UX] → Accept for Done; canvas later

## Migration Plan

1. Land ADR 0026 + OpenSpec change
2. Gate A: modifiers + method tracks + Import extract
3. Gate C: BlendSpace2D sampler + API
4. Gate D: Tree Asset + Inspector + override rules
5. Edit scrub; C-ABI / Blunder.Api
6. Engineering harnesses + lean Play bars; manual checklist
7. Keep Phase 1–4 content gates tracked
8. Rollback: disable modifiers/method/2D/Asset; Phase 4 tree embed path remains

## Open Questions

- Exact SkeletonModifier registration / C# subclass bridge (apply-time)
- Method event C-ABI / MessageId shape (apply-time)
- Instance-override allowlist (locked apply-time): `blendSpaceScalars`, `blendSpace2DParams`, `add2Weight`, `currentState`, `active`. Topology (points/states/base/Add2 clip/OneShot slot) lives on Asset or full embed.
- Import source for method tracks from glTF extras / companion (apply-time)
- Edit UI chrome for Asset picker vs inline Inspector (Done does not require polish alone)
