## 1. AnimationTree runtime core

- [x] 1.1 TDD: AnimationTree co-located with AnimationPlayer + Skeleton; resolves clips via player name→GUID map
- [x] 1.2 TDD: active tree exclusively samples Skeleton; Player Play/two-slot do not write bones while active; inactive restores Player path
- [x] 1.3 TDD: sample stack base then Add2; additive deltas relative to bind/rest
- [x] 1.4 TDD: BlendSpace1D neighbor blend (local TRS lerp / rotation slerp) from per-node scalar
- [x] 1.5 TDD: StateMachine Travel/Start selects named states (BlendSpace1D or single clip)
- [x] 1.6 TDD: OneShot inserts then returns to base
- [x] 1.7 TDD: PoseApplied fires under active tree; playback position follows base dominant clip; Add2 is not the step clock; TimeScale uses AnimationPlayer global

## 2. Sync Fire → OneShot

- [x] 2.1 TDD: Sync Group Fire on active-tree member applies OneShot; tree stays active
- [x] 2.2 TDD: Fire on no-tree member remains Phase 3 hard-cut Play
- [x] 2.3 TDD: mixed group (tree character + player prop) aligns at same logical moment

## 3. Scene embed + named API

- [x] 3.1 Persist scene-embedded topology (states, BlendSpace points, OneShot/Add2 slots); round-trip load
- [x] 3.2 Narrow named script/native API: Travel/Start, per-node BlendSpace scalar, RequestOneShot, Add2 setters (no parameter-path primary surface)
- [x] 3.3 Confirm Phase 4 does not require visual graph editor or standalone AnimationTree Asset

**Phase 4 Done scope (3.3):** Scene-embedded `animationTree` JSON on entities is sufficient. A visual node-graph editor and a standalone AnimationTree Asset type are explicitly **non-goals** for Phase 4 (see `design.md` Non-Goals, ADR 0025, `specs/animation-tree/spec.md` Scene-embedded topology).

## 4. Edit Mode preview

- [x] 4.1 Edit can activate AnimationTree and scrub named drives + TimeScale without DotNetHost / Behaviour Tick
- [x] 4.2 Verify Edit does **not** require Behaviour Tick or visual graph editor for scrub Done

**Phase 4 Edit scrub (4.2):** `AnimationPreviewController` + `tickObjectAnimationPreviewFrame` drive active-tree BlendSpace scalar, Travel/Start, OneShot, Add2 weight, and Player TimeScale with no Behaviour Tick and no visual graph editor — scene-embedded topology only (see task 3.3).

## 5. C-ABI + Blunder.Api

- [x] 5.1 C-ABI for AnimationTree activation + named drives; bump ABI; NativeAbi table
- [x] 5.2 Blunder.Api façades + completeness tests; Sync Fire OneShot semantics from managed API

## 6. Content gates

- [x] 6.1 Engineering gate: harness/mini scene for Travel, BlendSpace1D, OneShot (incl. Sync Fire→tree), Add2, exclusive sampling, PoseApplied/dominant clock, TimeScale, Edit scrub
- [ ] 6.2 Chocomel (or agreed subset) Play acceptance: perceptible BlendSpace motion, visible additive turn, OneShot return-to-base, stepped facing on base dominant clock
  - **Automated (engineering):** `dogwalk_phase4_mini_play_acceptance_test` — Play-path tick harness for BlendSpace scalar motion, Add2 overlay, OneShot return-to-base, dominant base clock, Sync Fire→OneShot.
  - **Human-pending:** Chocomel (or agreed subset) scene + `engine_player` Play pass (see `manual-checklist.md` §B). Full Chocomel content not required in worktree for engineering bar.
- [x] 6.3 Keep Phase 1–3 content Done gates tracked separately if still open (do not drop)

  Phase 1 Chocomel hard-cut (`dogwalk-animation-phase-1` tasks **6.4**), Phase 2 weighted (`dogwalk-animation-phase-2` tasks **5.3**), and Phase 3 mini Play (`dogwalk-animation-phase-3` tasks **5.2**) remain **open** if unfinished — not cancelled by Phase 4. See `manual-checklist.md` §C.

## 7. Docs / closeout

- [x] 7.1 Confirm CONTEXT + ADR 0025 match apply (prefer no churn)
- [x] 7.2 Manual checklist: Edit tree scrub; Play Chocomel-subset acceptance; earlier phase gates still tracked
