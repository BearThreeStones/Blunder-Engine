## 1. Scoped global pointer poll

- [x] 1.1 Add `DragOrigin` (or `dragNeedsGlobalPointerPoll()`) on `DockManager` / `DockDragController`, set on `beginDrag` from native float vs in-host
- [x] 1.2 Gate `SlintSystem::tickGlobalDockPointerPoll` so `handleDragMove` runs only when `dragNeedsGlobalPointerPoll()` is true; keep float resize poll unchanged
- [x] 1.3 Suppress native-float `tab-moved` → `handleDragMove` when global poll owns the drag (avoid dual writers)

## 2. Unified dock-local coordinates

- [x] 2.1 Implement `pointerToDockLocal(global_pt, docking_origin)` on `ChildWindowRegistry` or `DockFloatingWindowHost` (child rect → `floatLocalToDockLocal`, else `screenToDockLocal`)
- [x] 2.2 Use `pointerToDockLocal` in `tickGlobalDockPointerPoll` for native-float tab drags; verify HiDPI alignment with `window_pointer_map.h`
- [x] 2.3 Add debug-only log comparing poll coords vs Slint `tab-moved` at same screen position (optional, remove before archive if noisy)

## 3. Hide native float during tab drag

- [x] 3.1 Add `setFloatVisible(DockId, bool)` on `DockFloatingWindowHost` (SDL hide/show without destroying)
- [x] 3.2 Call hide on `beginDrag` when source is native float; restore on `cancelDrag` and on `endDrag` if float node still exists
- [x] 3.3 Confirm successful re-dock destroys float and does not restore hidden window

## 4. Guide slot stability

- [x] 4.1 Replace `computeSlot` float-equality tie-break with deterministic minimum-edge index
- [x] 4.2 Add hysteresis to `DockDragController` / `computeSlot` (`hysteresis_px`, `hysteresis_fraction` in `DockMetrics`)
- [x] 4.3 Verify guide `highlighted` and `previewRectForSlot` use the stabilized slot from `setHover`

## 5. Native float resize cursor

- [x] 5.1 Map `DockResizeEdge` to SDL system cursors in child window pointer path
- [x] 5.2 Restore default cursor when pointer leaves resize zones and on mouse release
- [x] 5.3 Add optional edge-strip hover tint in `floating_panel_window.slint` (match grip style)

## 6. Manual validation

- [x] 6.1 Main window: drag tab to container left edge — left guide stable, no center flicker
- [x] 6.2 Main window: merge two tabs into same tab bar — center guide stable on hover
- [ ] 6.3 Native float: drag tab to main dock (float moved aside) — guides + preview visible, center/edge drop succeeds
- [ ] 6.4 Native float: cancel tab drag — float window reappears, tab unchanged
- [ ] 6.5 Native float: hover resize edges — OS cursor changes; drag still resizes

## 7. Re-dock coords and Windows-style resize cursor (follow-up)

- [x] 7.1 `screenToDockLocal` applies `mapWindowPointerToLogical` (HiDPI-aligned with in-host tab drag)
- [x] 7.2 `pointerToDockLocal` skips `SDL_WINDOW_HIDDEN` children; tab-drag poll uses `screenToDockLocal` only
- [x] 7.3 `hitFloatResizeEdge` matches content-area edges (below title+tabs); title bar uses move cursor
- [x] 7.4 Remove edge-strip hover tint from `floating_panel_window.slint`

## 8. Child adapter lifecycle (crash fix)

- [x] 8.1 Add `unregisterChildAdapter` on `SlintPlatform` / `unregisterSlintChildAdapter` on `SlintSystem`
- [x] 8.2 Call unregister in `destroyEntry` before `component->reset()`
- [x] 8.3 Roll back adapter registration in `createEntry` catch paths
- [x] 8.4 Skip `makeFloatingFor` on `endDrag` when tab drag originated from native float
- [x] 8.5 Skip hidden / invalid SDL windows in `renderFrames`
