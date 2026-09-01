## 1. Preview clip on the controller

- [x] 1.1 Rename session Fire-target to Preview clip (`m_fire_target` / Slint property) or alias it; Play with empty name Clip Plays Preview clip, not `m_default_clip_name`
- [x] 1.2 Preview clip setter stores the name, activates an inactive tree, `clipPlay`s at 0, and does not change transport state
- [x] 1.3 Bind on Object change: Preview clip = scene default (else first binding name), activate if needed, Clip Play at 0, transport Stopped; same-object bindSelection does not Clip Play again
- [x] 1.4 `stop()` seeks 0, clears Fire, Ends CINE, does not `clearClipPlay`; `haltBoundSession` / `clearTarget` still `clearClipPlay` on the tree being left

## 2. Tests

- [x] 2.1 Play uses Preview clip when it differs from the scene default
- [x] 2.2 Changing Preview clip hard-cuts in Playing, Paused, and Stopped (Stopped does not start transport)
- [x] 2.3 Stop keeps Clip Play override at clock 0; Fire occupying then changing Preview clip keeps the insert and retargets the Clip Play base
- [x] 2.4 Bind Clip Plays the default at 0; rebind / Camera selection clears the previous tree's override; Preview clip does not dirty Document History
- [x] 2.5 Run the updated test name (`ctest` or the test executable). Compiling the `*_test` target is not a Test run

## 3. Window chrome

- [x] 3.1 Combo change callback uses the Preview clip setter (no second dropdown)
- [x] 3.2 Fire still inserts Preview clip on the Fire slot

## 4. Validation

- [x] 4.1 `openspec validate animation-window-preview-clip --strict`
- [x] 4.2 Build `engine_editor`
- [x] 4.3 Human acceptance: walk `manual-checklist.md` in the windowed editor (not Agent QC)
