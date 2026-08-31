## 1. Cover helper

- [x] 1.1 Add a windowed-Editor-only Startup cover helper: phase enum mapped to Cooking assets / Preparing editor / Starting editor, wordmark from `readProjectFile`, Base 2 fill
- [x] 1.2 Windows: paint that helper on the session HWND (GDI or equivalent); no SDL_Renderer; no Slint until the shared Vulkan device exists
- [x] 1.3 Pump SDL events (quit, resize, expose) while the cover is up without running `tickOneFrame`

## 2. Boot order

- [x] 2.1 In windowed Editor `startSystems`, create and show the session window after FileSystem + Project display name, before `cookIfStale` and Vulkan
- [x] 2.2 Set stage names at cook / remaining editor Systems / Vulkan+Slint boundaries; skip a name if that phase does not run
- [x] 2.3 Close during cover aborts boot and ends the process with no confirm; Headless / CLI / MCP / Player / Project Manager never mount the cover
- [x] 2.4 Dismiss native paint when the Editor Shell has presented once (not merely when `MainEditorWindow` is created); no minimum dwell

## 3. Tests

- [x] 3.1 First-party test: cover mounts only for windowed Editor; Headless / Player gating
- [x] 3.2 First-party test: phase enum maps to the three English stage names
- [x] 3.3 Wire the test in `engine/src/tests/CMakeLists.txt` and run it (`ctest` or the executable). Compiling the `*_test` target is not a Test run

## 4. Validation

- [x] 4.1 `openspec validate startup-cover --strict`
- [x] 4.2 Build `engine_editor` (Windows Debug preset)
- [x] 4.3 Human acceptance: walk `manual-checklist.md` in the windowed editor (not Agent QC)
