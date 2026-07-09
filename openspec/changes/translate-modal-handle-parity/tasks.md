## 1. Session + screen-space motion (TDD)

- [x] 1.1 Add `TranslateModalSession` (header/cpp) with begin-from-handle, pointer-move, confirm, cancel, and feedback query API
- [x] 1.2 Implement screen-delta → world view-plane delta using `EditorCamera` (Blender `convertViewVec` analogue)
- [x] 1.3 Implement axis / infinite-plane / free-center constraint projection + view-aligned axis fallback
- [x] 1.4 Add unit tests for axis, plane (infinite), and center deltas at fixed camera poses; watch RED then GREEN
- [x] 1.5 Register new sources in render CMake and test target in `engine/src/tests/CMakeLists.txt`

## 2. Wire handle entry + confirm/cancel

- [x] 2.1 Route translate LMB press on axis/plane/center into `TranslateModalSession::beginFromHandle`
- [x] 2.2 Drive `onMouseMoved` translate path through the session (remove finite plane-quad tracking for translate)
- [x] 2.3 Confirm on LMB release; cancel on Escape / RMB with restore of drag-start entity TRS
- [x] 2.4 Keep inspector live sync + `notifyViewportAfterGizmoTransformEdit` / scene dirty on session updates
- [x] 2.5 Ensure rotate/scale handle paths remain unchanged

## 3. Drag feedback: cursor, guides, ghost, visibility, origin dot

- [x] 3.1 Set/restore four-way move cursor for session lifetime (no hover cursor change)
- [x] 3.2 Draw constraint guides pinned at drag-start (1 axis / 2 plane / none free)
- [x] 3.3 Draw active-handle-only drag ghost at drag-start pose (including center ghost when free-moving)
- [x] 3.4 Apply handle visibility: reference axis arrows follow live pivot; hide planes on axis drag; keep only active plane on plane drag; hide solid center during session
- [x] 3.5 Draw origin dot (axis root / plane handle with normal-axis color)
- [x] 3.6 Request viewport redraw whenever session feedback state changes

## 4. Transform-active outline

- [x] 4.1 Expose session-active (or translate-drag-active) to outline resolve
- [x] 4.2 Use fixed light/off-white transform-active outline color while translate session active; restore `#F57011` on end
- [x] 4.3 Verify selection-cleared-during-drag disables outline (existing scenario)

## 5. Docs + validation

- [x] 5.1 Note translate modal handle feedback path in `docs/agents/render-pipeline.md`
- [x] 5.2 Confirm `CONTEXT.md` translate terms still match implementation names
- [ ] 5.3 Build `engine_runtime` / relevant tests; run motion unit tests
- [ ] 5.4 Manual QA (static camera): axis / plane / center drag feel; guides pinned; ghost; visibility; origin dot; cursor; outline; Escape/RMB cancel; LMB release confirm
- [x] 5.5 `openspec validate translate-modal-handle-parity`
