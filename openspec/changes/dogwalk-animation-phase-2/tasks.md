## 1. Runtime two-slot + TimeScale

- [ ] 1.1 Extend AnimationPlayer with slot0/slot1 clip assignment, blendWeight ∈ [0,1], and global TimeScale
- [ ] 1.2 Sample both active slots and combine local TRS (lerp) / rotation (slerp) onto the co-located Skeleton
- [ ] 1.3 Implement Crossfade as time-driven weight ramp on the two slots; Play(name, fade) sugar; fade=0 hard cut
- [ ] 1.4 Advance slots under global TimeScale; expose dominant-slot playback position for step sync
- [ ] 1.5 Keep PoseApplied after combined sample; preserve Tick → sample → PoseApplied order

## 2. Scene + Edit authorship

- [ ] 2.1 Serialize/deserialize Phase 2 defaults (TimeScale, optional default slots + initial weight) with name→GUID map
- [ ] 2.2 Edit Mode controls to scrub TimeScale, fade, and two-slot weights without DotNetHost / Behaviour Tick
- [ ] 2.3 Verify Edit scrub does not run Behaviour Tick

## 3. C-ABI + Blunder.Api

- [ ] 3.1 Add C-ABI entry points for SetSlot, blendWeight, TimeScale, Play-with-fade; bump ABI version; fill NativeAbi
- [ ] 3.2 Extend Blunder.Api AnimationPlayer + NativeAbi completeness for the new entries
- [ ] 3.3 Tests: C-ABI / NativeAbi completeness and managed façade smoke

## 4. Automated validation

- [ ] 4.1 Unit tests: weighted dual-track pose combine; Crossfade ramp; fade=0 hard cut
- [ ] 4.2 Unit tests: TimeScale advances both slots; dominant-slot playback position under blend
- [ ] 4.3 Scene defaults round-trip test for TimeScale / slots / weight

## 5. Content gates

- [ ] 5.1 Engineering gate: test-rig (or equivalent) demonstrates two-slot blend, Crossfade, TimeScale, dominant-slot step sync
- [ ] 5.2 Wire Test Project C# to SetSlot + blendWeight (idle↔walk) while Position stays Tick real-time; stepped facing uses dominant clock
- [ ] 5.3 Chocomel (or agreed subset) Play acceptance: weighted idle↔walk, perceptible TimeScale, real-time move, stepped facing — Phase 2 Done
- [ ] 5.4 Keep Phase 1 Chocomel hard-cut acceptance tracked separately if still open (do not drop)

## 6. Docs / closeout

- [ ] 6.1 Confirm CONTEXT + ADR 0020 match apply (prefer no churn)
- [ ] 6.2 Manual checklist: Edit scrub; Play weighted blend + TimeScale; Fast Path/Final skin still sane under blend
