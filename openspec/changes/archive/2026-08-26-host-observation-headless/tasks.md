## 1. Launch flag and composition

- [x] 1.1 Parse `--headless` on `engine_editor` and `engine_player`; pass a `headless` bool into `startSystems` beside `EngineHostMode` (no third enum)
- [x] 1.2 Headless Editor: skip `WindowSystem` SDL window, Slint, UiHost, viewport sink/bridge; still mount Authorship System, Scene, Capture still path, and `PlaySessionController`
- [x] 1.3 Tests: Headless Editor boots with Authorship mounted and no window; Headless Player boots with Authorship not mounted and no window

## 2. Headless Vulkan and Player present-skip

- [x] 2.1 Headless Vulkan device with no `VkSurfaceKHR` / swapchain; fail closed if the device cannot be created
- [x] 2.2 Headless Player: offscreen color target at Capture aspect; reuse `capturePlayProcessFrame` (not HWND, not Scene Thumbnail instantiate)
- [x] 2.3 Windowed Player window-close-as-Stop unchanged; Headless Player ends via Stop or process exit

## 3. Play session and observation

- [x] 3.1 Headless Editor Play spawn adds `--headless` to Player argv; windowed Play still spawns windowed Player
- [x] 3.2 Headless Play with a dirty Live document uses last saved asset (no prompt, no auto-save)
- [x] 3.3 Tests: Headless Capture is 16:9 Scene still without a window; Pause + Play step + Play frame over the existing control channel (wait on poll until frame or timeout)

## 4. Docs

- [x] 4.1 Confirm CONTEXT Headless / ADR 0043 match the shipped boot flag (no extra glossary). Document the Headless test target in `docs/agents/testing.md` if it needs GPU/PATH
