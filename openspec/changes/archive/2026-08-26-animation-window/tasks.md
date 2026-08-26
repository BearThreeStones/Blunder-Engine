## 1. Dock panel

- [x] 1.1 Add `DockPanelKind::animation` and thread it through dock snapshot / tile `active-panel-kind` like Hierarchy
- [x] 1.2 Create widget titled Animation in default layout, split under the viewport; inject-if-missing for saved layouts
- [x] 1.3 Empty Slint panel in the dock tile (tab title Animation, leading Animation.svg optional)

## 2. Bind and transport

- [x] 2.1 Bind `AnimationPreviewController` on single selection with AnimationTree; empty/multi/no-Tree → unbound (dock stays open)
- [x] 2.2 Rebind Stops the previous tree: seek 0, clear Fire slot, halt transport, End CINE
- [x] 2.3 Play activates inactive tree then advances; Pause freezes; Stop seeks 0, clears Fire slot, Ends CINE, tree stays active
- [x] 2.4 Session Loop wraps current ruler clip while Playing; Loop off pauses last frame; Play from that Pause starts at 0
- [x] 2.5 Ruler/playhead use base dominant clip, or insert clip while Fire/OneShot occupies; scrub seeks that clock

## 3. Fire and CINE

- [x] 3.1 Fire-target dropdown lists bound tree Clip Binding logical names; Fire calls `requestOneShot` (hard-cut); do not call `fireAnimationSyncPreview`
- [x] 3.2 Enter sets in-CINE + Inp session flags only (no Fire, no seek); End clears flags only; Stop/rebind End CINE

## 4. TimeScale

- [x] 4.1 Window TimeScale reads/writes the rate the tree already samples; commit dirties Document History; dual-track Inspector when that row exists
- [x] 4.2 Transport / playhead / Loop / Fire occupancy / CINE do not dirty

## 5. Chrome

- [x] 5.1 Icon-first Play/Pause/Stop/Loop/Fire/Enter/End (ADR 0042); EditorSlider for TimeScale; CINE/Inp word badges
- [x] 5.2 Ruler + playhead only (no track rows, no key diamonds); clock name + time readout
- [x] 5.3 Unbound: disable transport and timeline (panel still visible)

## 6. Remove overlay

- [x] 6.1 Delete `AnimationPreviewToolbar` from the viewport; drop S0/S1/BW/Fd wiring
- [x] 6.2 Update `modern-dark-editor-theme` editor-shell so animation preview is not required as a viewport overlay

## 7. Validation

- [x] 7.1 Tests: Tree bind enable/disable; rebind Stop; Loop off last-frame Pause; Fire hard-cut; Enter does not Fire; TimeScale dirty; Play does not dirty
- [x] 7.2 Build `engine_editor` (kill running editor if LNK1168)
- [x] 7.3 Manual: Chocomel enables window; Camera disables; Play/Fire/CINE/TimeScale match mockup; no overlay S0 toolbar; transform strip stays
