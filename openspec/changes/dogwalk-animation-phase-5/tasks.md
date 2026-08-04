## 1. Gate A — SkeletonModifier

- [x] 1.1 TDD: SkeletonModifier runs after Player/Tree sample and before PoseApplied
- [x] 1.2 TDD: ordered modifier chain on same Object
- [x] 1.3 TDD: extension point + test double modifier
- [x] 1.4 TDD: minimal LookAt/aim sample modifier produces visible post-pose change
- [x] 1.5 Confirm Add2 remains in-tree additive and distinct from SkeletonModifier

## 2. Gate A — Method tracks

- [ ] 2.1 TDD: AnimationClip YAML stores method keys (name + time + optional args)
- [ ] 2.2 TDD: Import/extract path preserves method tracks (glTF extras / companion as applicable)
- [ ] 2.3 TDD: key-crossing dispatch on base dominant-clip clock to co-located Behaviours (Message optional)
- [ ] 2.4 TDD: while OneShot active, method dispatch uses OneShot clock
- [ ] 2.5 Confirm engine does not PtrCall arbitrary C# methods by string as product path

## 3. Gate C — BlendSpace2D

- [ ] 3.1 TDD: BlendSpace2D points + triangulation; barycentric TRS lerp/slerp
- [ ] 3.2 TDD: per-node `(x,y)` named API
- [ ] 3.3 TDD: StateMachine state may use BlendSpace2D as playback
- [ ] 3.4 TDD: dominant-clip clock under BlendSpace2D (define tie-break)

## 4. Gate D — AnimationTree Asset

- [ ] 4.1 TDD: AnimationTree Asset GUID body round-trip (topology)
- [ ] 4.2 TDD: AnimationTree references Asset; runtime uses Asset as base
- [ ] 4.3 TDD: small scene instance overrides apply on top of Asset (allowlist)
- [ ] 4.4 TDD: no Asset reference → Phase 4 embedded topology still works
- [ ] 4.5 Inspector authorship for Asset topology (no visual canvas required for Done)
- [ ] 4.6 Confirm visual canvas is not required for Phase 5 D Done

## 5. Edit Mode preview

- [ ] 5.1 Edit can enable/order modifiers and see post-pose without Behaviour Tick
- [ ] 5.2 Edit can scrub BlendSpace2D `(x,y)` without Behaviour Tick
- [ ] 5.3 Edit can bind/edit Tree Asset + overrides and preview without Behaviour Tick
- [ ] 5.4 Method scrub may show markers/logs; real Behaviour handling remains Play-validated

## 6. C-ABI + Blunder.Api

- [ ] 6.1 C-ABI for modifiers / method hooks / BlendSpace2D / Tree Asset; bump ABI; NativeAbi table
- [ ] 6.2 Blunder.Api façades + completeness tests

## 7. Content gates

- [ ] 7.1 Engineering gate A harness (modifier + method)
- [ ] 7.2 Engineering gate C harness (BlendSpace2D)
- [ ] 7.3 Engineering gate D harness (Asset + Inspector + overrides)
- [ ] 7.4 Lean Play A: visible modifier effect + observable method dispatch
- [ ] 7.5 Lean Play C: perceptible 2D blend (Pinda subset or test field OK)
- [ ] 7.6 Lean Play D: Asset reference + Inspector + override without Behaviour
- [ ] 7.7 Keep Phase 1–4 content Done gates tracked separately if still open

  Phase 4 Chocomel-subset (`dogwalk-animation-phase-4` **6.2**), Phase 3 mini Play, Phase 2 weighted, Phase 1 hard-cut remain open if unfinished — not cancelled by Phase 5.

## 8. Docs / closeout

- [ ] 8.1 Confirm CONTEXT + ADR 0026 match apply (prefer no churn)
- [ ] 8.2 Manual checklist: Edit A/C/D scrub; Play lean bars; earlier gates tracked
