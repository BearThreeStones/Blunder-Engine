## 1. LookAt product

- [x] 1.1 TDD: configurable bone name + target (and apply-time min field set) on LookAt
- [x] 1.2 TDD: Edit scrub LookAt without Behaviour Tick
- [x] 1.3 Confirm sample-era LookAt remains a valid ClassDB product path (no regression)

## 2. PaperMouth

- [x] 2.1 TDD: PaperMouth applies jaw pose from `openAmount`
- [x] 2.2 TDD: configurable jaw bone name
- [x] 2.3 TDD: optional attach-driven mode fills `openAmount` when enabled; default off
- [x] 2.4 TDD: Edit scrub `openAmount` without Behaviour Tick

## 3. SkeletonAttachModifier

- [x] 3.1 TDD: host bone world transform copies to child Object Transform after sample
- [ ] 3.2 TDD: invalid child/bone fails safely
- [ ] 3.3 Confirm Attach does not drive a remote Skeleton
- [ ] 3.4 TDD: Edit preview shows child follow without Behaviour Tick

## 4. Authorship / persistence

- [ ] 4.1 TDD: scene round-trip for PaperMouth, Attach, LookAt (type, order, key params)
- [ ] 4.2 Inspector: add/remove/reorder/enable + edit key params for the three types
- [ ] 4.3 Confirm no visual Tree canvas required for Phase 6 Done
- [ ] 4.4 Confirm generic C# SkeletonModifier subclass hot-bridge is not Phase 6 Done

## 5. C-ABI + Blunder.Api

- [ ] 5.1 Lean C-ABI for `openAmount`, Attach child/bone, LookAt target/bone; bump ABI; NativeAbi table
- [ ] 5.2 Blunder.Api façades + completeness tests for lean drives only

## 6. Content gates

- [ ] 6.1 Engineering harness: PaperMouth + Attach + LookAt product
- [ ] 6.2 Lean Play/Edit: visible mouth open, child follow, aim change
- [ ] 6.3 Keep Phase 1–5 content Done gates tracked separately if still open

  Phase 5 lean Play / Phase 4 Chocomel-subset / earlier bars remain open if unfinished — not cancelled by Phase 6.

## 7. Docs / closeout

- [ ] 7.1 Confirm CONTEXT + ADR 0027 match apply (prefer no churn)
- [ ] 7.2 Manual checklist: Edit scrub three modifiers; lean Play; earlier gates tracked
