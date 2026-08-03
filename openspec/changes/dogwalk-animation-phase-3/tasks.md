## 1. Sync Group runtime

- [x] 1.1 TDD: create / join / leave / destroy Sync Group; members are AnimationPlayers
- [x] 1.2 TDD: Fire with per-member `(player, clipName[, seek])`; heterogeneous names; same logical moment start (hard cut)
- [x] 1.3 TDD: same-name Fire sugar (optional) resolves via each player’s name→GUID map
- [ ] 1.4 TDD: Fire does not use Crossfade by default; fade=0 / snap semantics under Phase 2 player
- [ ] 1.5 Confirm Phase 1 rule: each Player still drives only co-located Skeleton (no remote Skeleton via Sync Group)

## 2. CINE segment contract

- [ ] 2.1 TDD: Enter CINE sets in-CINE mark; End clears it (explicit End authoritative)
- [ ] 2.2 TDD: optional gameplay-input suppression while in-CINE; restores on End
- [ ] 2.3 TDD: member `finished` may signal but does not alone End the segment
- [ ] 2.4 Document that pose snap / gameplay state transitions remain C# Behaviour responsibilities

## 3. Edit Mode preview

- [ ] 3.1 Edit can Fire a Sync Group and Enter/End CINE without DotNetHost / Behaviour Tick
- [ ] 3.2 Edit shows in-CINE / suppression marking and multi-Skeleton playback
- [ ] 3.3 Verify Edit does **not** auto-snap Object TRS or run gameplay state machines

## 4. C-ABI + Blunder.Api

- [ ] 4.1 C-ABI for Sync Group lifecycle + Fire; CINE Enter/End / in-CINE query; bump ABI; NativeAbi table
- [ ] 4.2 Blunder.Api façades + completeness tests

## 5. Content gates

- [ ] 5.1 Engineering gate: multi-Player Sync Group Fire + CINE Enter/End on a test harness / mini scene
- [ ] 5.2 Mini Play acceptance: character + prop/partner synchronized start; CINE handoff returns control (simplified props OK)
- [ ] 5.3 Keep Phase 2 Chocomel weighted acceptance tracked separately if still open (do not drop)

## 6. Docs / closeout

- [ ] 6.1 Confirm CONTEXT + ADR 0023 match apply (prefer no churn)
- [ ] 6.2 Manual checklist: Edit Fire/CINE marks; Play mini SYNC+CINE acceptance; Phase 2 gate still tracked
