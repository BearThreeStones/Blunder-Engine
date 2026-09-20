# Tasks

## 1. Overlay chrome and clock

- [ ] 1.1 Add an authored Editor modal (title `Opening editor`, percent, stage caption, elapsed seconds, no Cancel/Retry) on the windowed Editor Shell after Startup cover dismiss, and verify a first-party test (or equivalent) constructs the overlay model without an OS window
- [ ] 1.2 Count elapsed whole seconds from `startupCoverBegin` (first OS window) and verify the overlay readout does not restart at overlay show
- [ ] 1.3 Keep Startup cover free of percent and elapsed, and verify `startup_cover_test` still maps Cooking assets / Preparing editor / Starting editor with no percent

## 2. Stages, percent, dismiss

- [ ] 2.1 Map overlay captions to **Opening scene** (`loadScene` deserialize + instantiate) and **Indexing content** (`ContentBrowserSystem::refresh` scan), with fixed weights Cooking 40 / Preparing 15 / Starting 15 / Opening scene 20 / Indexing content 10, and verify percent is 70 when the overlay appears after cover stages
- [ ] 2.2 Jump percent only at those stage boundaries (90 after Opening scene, 100 after Indexing content) and verify the test does not lerp between weights
- [ ] 2.3 Dismiss when index scan + startup entity table are done so product Iterate can run, and verify dismiss does not wait Mesh Loader `GpuMesh`, Texture Loader copies, or thumbnail 2/tick

## 3. Pump, hosts, session-start

- [ ] 3.1 Pump SDL events and Present Shell+overlay during remaining `initialize` / first-tick index scan, and verify close while overlay is up ends the Editor Session with no Retry
- [ ] 3.2 Skip the overlay for Headless / CLI / MCP / Project Manager / Player, and verify host-gating tests match windowed Editor only
- [ ] 3.3 Do not re-show the overlay when opening another Scene Asset from the Content Browser after product Iterate, and verify a second `openScene` in the test fixture leaves overlay dismissed

## 4. DogWalk contract (apply later; not this planning commit)

- [ ] 4.1 Human walk remains DogWalk `assets/Scenes/se-world.scene.asset` in windowed `engine_editor` (not Test/Sponza, not collision QC `root.scene.asset`, not Player/PM); checklist stays **Not run** until the human walks it
- [ ] 4.2 Do not change cook, Mesh Loader budget, Texture Loader, or flatten, and verify this change’s diff has no Mesh Loader / Texture Loader algorithm edits
- [ ] 4.3 Do not edit `cursor/scene-collision-bridge-8a89` or PR 24; verify this change’s diff has no files from that branch

## 5. Docs and validate

- [ ] 5.1 Keep `CONTEXT.md` Editor open progress / Startup cover terms aligned with implementation names; ADR 0073 stays the overlay vs cover vs splash record
- [ ] 5.2 `openspec validate engine-open-progress --strict` passes
