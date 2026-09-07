## 1. Anatomy model

- [x] 1.1 Add `buildClipAnatomy` (or equivalent) that groups `AnimationClipData` tracks by first bone appearance and orders Position → Rotation → Scale, omitting missing channels and ignoring method keys
- [x] 1.2 Apply session bone-name substring filter on the grouped view (hide unmatched groups; do not match on channel words)
- [x] 1.3 Session collapsed-bone set; clear and re-expand when the ruler clip name changes
- [x] 1.4 Resolve the current ruler clip through the bound tree Clip Binding + `resolveAnimationClipFromAssets`; empty/unbound → no rows

## 2. Tests

- [x] 2.1 First-appearance grouping + TRS order (including interleaved tracks and missing Scale)
- [x] 2.2 Filter hides unmatched groups; fold reset on ruler clip name change
- [x] 2.3 Extra clip bone still listed with no Skeleton consult; method keys do not create rows
- [x] 2.4 Run the new/updated test name (`ctest` or the test executable). Compiling the `*_test` target is not a Test run

## 3. Animation Window UI

- [x] 3.1 Add `EditorGlyphs` `track-position` / `track-rotation` / `track-scale` from Godot `KeyTrack*.svg` without gray `colorize`
- [x] 3.2 Split `AnimationWindowPanel`: left filter + fold + bone/channel names; right ruler + key lanes; diamonds at key times (not selectable/draggable)
- [x] 3.3 One playhead through ruler and visible clip-track rows; TouchArea on the timeline column only (name/filter/fold do not seek)
- [x] 3.4 Wire anatomy properties on `editor_window.slint` and `floating_panel_window.slint`; hide anatomy when unbound
- [x] 3.5 Filter and fold do not dirty Document History

## 4. Validation

- [x] 4.1 `openspec validate animation-window-clip-anatomy --strict`
- [x] 4.2 Build `engine_editor`
- [x] 4.3 Human acceptance: walk `manual-checklist.md` in the windowed editor (not Agent QC)
