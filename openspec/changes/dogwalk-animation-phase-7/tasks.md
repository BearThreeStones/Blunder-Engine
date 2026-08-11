## 1. Runtime — tree params + transitions

- [x] 1.1 TDD: AnimationTree independent bool/float tree parameters get/set by name
- [x] 1.2 TDD: StateMachine transition edge with single predicate auto-Travels (hard-cut)
- [x] 1.3 TDD: hybrid condition inputs — named drive ref and independent tree param
- [x] 1.4 TDD: multi-true outgoing edges select highest author priority (stable tie-break)
- [x] 1.5 TDD: script Travel/Start still forces state when no edge fires
- [x] 1.6 TDD: active tree evaluates transitions on animation advance path

## 2. AnimationTree Asset persistence

- [x] 2.1 TDD: Asset body round-trips transitions (target, predicate, priority)
- [x] 2.2 TDD: Asset body round-trips tree parameter declarations/defaults as applicable
- [x] 2.3 TDD: Asset body round-trips Canvas layout (node positions)
- [x] 2.4 TDD: Inspector-only Asset edit still works without Canvas (Phase 5 D preserved)

## 3. AnimationTree Canvas UI

- [ ] 3.1 Canvas document opens AnimationTree Asset; edits topology on Asset body
- [ ] 3.2 Author StateMachine, BlendSpace1D, BlendSpace2D, OneShot, Add2 on canvas
- [ ] 3.3 Author transition edges (predicate, priority, target) on canvas
- [ ] 3.4 Dual-track: Inspector remains writable for same Asset topology
- [ ] 3.5 Open path: Content Browser → AnimationTree Asset → Canvas
- [ ] 3.6 Open path: Object Inspector (Asset reference) → Canvas (same GUID)
- [ ] 3.7 Confirm bound live preview / in-canvas mini viewport is **not** required for Phase 7 Done

## 4. C-ABI + Blunder.Api

- [ ] 4.1 C-ABI get/set tree parameters; bump ABI; NativeAbi table
- [ ] 4.2 Blunder.Api façades + completeness tests; Travel/Start remain available

## 5. Content gates

- [ ] 5.1 Engineering gate: canvas Asset round-trip (edges + layout); condition auto Travel; priority; Travel coexist; dual open paths; Inspector dual-track
- [ ] 5.2 Lean Play: perceptible automatic state change via transition conditions (test rig or agreed subset)
- [ ] 5.3 Keep Phase 1–6 content Done gates tracked separately if still open

  Phase 6 modifier Play / Phase 5 lean Play / Phase 4 Chocomel-subset / earlier bars remain open if unfinished — not cancelled by Phase 7.

## 6. Docs / closeout

- [ ] 6.1 Confirm CONTEXT + ADR 0030 match apply (prefer no churn)
- [ ] 6.2 Manual checklist: Canvas authorship; condition Play bar; earlier gates tracked; follow-ups listed (bound preview, clip-end edges, edge fade)
