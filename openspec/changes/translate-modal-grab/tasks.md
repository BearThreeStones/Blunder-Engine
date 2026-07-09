## 1. Session grab entry + confirm policy (TDD)

- [ ] 1.1 Add session entry kind (`handle` vs `grab`) and `beginFromGrab` (free view-plane constraint + start TRS/pivot snapshot)
- [ ] 1.2 Add confirm-policy helper: grab confirms on LMB press; handle remains LMB release
- [ ] 1.3 Unit tests: grab begin is free/unconstrained; grab confirm policy vs handle; cancel restores start pose; watch RED then GREEN

## 2. Wire G key + grab confirm/cancel

- [ ] 2.1 Route `G` (selection present, no active session) into `beginFromGrab` + full TRS snapshot + camera lock
- [ ] 2.2 While grab-active, consume LMB press to confirm before pick / handle hit tests
- [ ] 2.3 Cancel grab on Escape / RMB with TRS restore (reuse P1 restore path)
- [ ] 2.4 Keep inspector live sync + scene dirty + viewport redraw on grab move/confirm/cancel
- [ ] 2.5 Ensure handle release-to-confirm path remains unchanged

## 3. Grab feedback

- [ ] 3.1 Set/restore four-way move cursor for grab session lifetime
- [ ] 3.2 Apply grab visibility: hide solid center; keep reference axis arrows on live pivot; no guides; no handle ghost; no origin dot
- [ ] 3.3 Drive transform-active outline while grab session active (reuse P1 session-active flag)
- [ ] 3.4 Request viewport redraw on grab begin/move/end

## 4. Docs + validation

- [ ] 4.1 Add **Grab entry** term to `CONTEXT.md` under Transform gizmo (translate)
- [ ] 4.2 Note grab entry / click-to-confirm in `docs/agents/render-pipeline.md`
- [ ] 4.3 Build/run grab-related unit tests
- [ ] 4.4 Manual QA: `G` free move; LMB click confirm; Esc/RMB cancel; cursor; outline; no guides/ghost; handle path still release-confirm
- [ ] 4.5 `openspec validate translate-modal-grab`
