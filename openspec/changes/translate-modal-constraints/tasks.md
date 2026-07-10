## 1. Session constraint + orientation API (TDD)

- [x] 1.1 Extend `TranslateModalSession` with live constraint kind, orientation (global/local), and re-press cycle state
- [x] 1.2 Implement axis/plane key handlers: first press → global; same key → local; same key again → free; different key → global for new constraint
- [x] 1.3 Rebuild constraint basis from session-start rotation (local) or world axes (global); re-project from session-start TRS + current pointer after each change
- [x] 1.4 Unit tests: X/Y/Z cycle, Shift plane keys, grab then constrain, local vs global deltas at fixed poses; watch RED then GREEN

## 2. Numeric input (TDD)

- [x] 2.1 Add numeric buffer API on the session (append digit/`-`/`.`, backspace, clear, parse distance)
- [x] 2.2 While numeric active, ignore pointer motion for position; apply signed distance along active constraint
- [x] 2.3 Unit tests: axis distance `2`, negative/decimal, clear restores pointer path; watch RED then GREEN

## 3. MMB axis pick (TDD + wire)

- [x] 3.1 Add MMB pick state: begin/update/end; nearest projected axis from mouse delta (Blender `setNearestAxis3d` analogue)
- [x] 3.2 On MMB release, commit single-axis constraint and continue the session
- [x] 3.3 Unit tests for nearest-axis selection with fixed screen deltas; watch RED then GREEN
- [x] 3.4 Wire MMB in `TransformGizmoController` while session active; block camera orbit

## 4. Controller input routing

- [x] 4.1 While session active, route `X`/`Y`/`Z` (+ Shift) into session constraint handlers (not mode switches)
- [x] 4.2 Route digits/`-`/`.`/Backspace into numeric input; Escape clears numeric first, then cancels
- [x] 4.3 Route Enter to confirm (handle and grab)
- [x] 4.4 Keep inspector sync, scene dirty, and viewport redraw on constraint/numeric/MMB updates

## 5. Live constraint feedback

- [ ] 5.1 Update guides / origin dot / plane-center visibility from **live** constraint (including grab free→constrained)
- [ ] 5.2 Keep handle ghost tied to originally pressed handle only; grab never shows handle ghost
- [ ] 5.3 Optional MMB-pick preview guides (three axes or nearest highlight) through drag-start pivot

## 6. Docs + validation

- [ ] 6.1 Add CONTEXT.md terms: constraint orientation cycle, MMB axis pick, numeric input
- [ ] 6.2 Note P3 constraint/MMB/numeric behavior in `docs/agents/render-pipeline.md`
- [ ] 6.3 Build/run constraint-related unit tests
- [ ] 6.4 Manual QA: handle + grab with X/Y/Z cycle, Shift planes, MMB pick, numeric + Escape order, Enter confirm, feedback updates
- [ ] 6.5 `openspec validate translate-modal-constraints`
