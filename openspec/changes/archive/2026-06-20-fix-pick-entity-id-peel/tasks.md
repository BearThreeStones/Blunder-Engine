## 1. PickOverlay entity-ID peel

- [x] 1.1 Add helper to filter `PickDraw` list by excluded `EntityId` set
- [x] 1.2 Refactor `pickAllAtWindowPosition` to iterate: filtered draws → readback → append → exclude entity (no `is_peel_pass`)
- [x] 1.3 Add `recordPixelPickPass` overload or parameter for pre-filtered draws (reuse existing path)

## 2. HybridGpuPickSystem

- [x] 2.1 Replace depth peel state (`m_peel_depth`, peel shader flags) with `m_excluded_entities` set
- [x] 2.2 On each fence complete: append hit, add to excluded, submit next pass with filtered draws or finish
- [x] 2.3 Stop chain on miss/empty draws only; remove duplicate-entity as chain terminator

## 3. Pick-test scene

- [x] 3.1 Set `BoxFront`/`BoxMid`/`BoxBack` to Y positions `2.0` / `2.9` / `3.8` (0.9 m spacing, overlapping 1 m cubes)
- [x] 3.2 Update `docs/agents/render-pipeline.md` pick-test table and document entity-ID peel

## 4–9. Prior revisions (idle poll, multi_peel, poll-before-render, menu isolation)

- [x] 4.x–9.x Implemented (see git history / prior tasks)

## 10. Build and validation (prior)

- [x] 10.1 Build `engine_editor`
- [x] 10.2 Manual: Ctrl+right piercing menu ≥3 rows (±Y view)
- [x] 10.3 Manual: same-pixel left-click cycles ≥3 hits
- [x] 10.4 Manual: first left-click → outline + gizmo without camera move
- [x] 10.5 Manual: piercing menu row → outline + gizmo persist

## 11. Sync-then-async left-click (Solution A)

- [x] 11.1 On `onViewportLeftReleased` (new `multi_peel` path): sync front-most pick + `promotePickEntity` + immediate `applySelection`
- [x] 11.2 Then `requestPick(multi_peel)` as today (do not block on async)
- [x] 11.3 `deliverPickResult`: for left-click `multi_peel`, update `m_last_peel_hits` / pixel; skip `applySelection` when promoted front equals current primary
- [x] 11.4 Async-only fallback: if sync missed and async has hits, `applySelection` from async front
- [x] 11.5 Local same-pixel cycle branch unchanged (no sync pick on that path)

## 12. Docs

- [x] 12.1 Update `docs/agents/render-pipeline.md` with sync-then-async left-click flow

## 13. Build and validation

- [x] 13.1 Build `engine_editor`
- [x] 13.2 Manual 10.4: first left-click → outline + gizmo without camera move (≥200ms wait)
- [x] 13.3 Regression: 10.3 cycle and 10.5 menu still pass
