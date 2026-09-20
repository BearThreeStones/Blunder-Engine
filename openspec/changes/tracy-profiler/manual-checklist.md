# Manual checklist — tracy-profiler

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Not run

Windowed `engine_editor` and Play on the DogWalk Project (`E:\Blunder Projects\DogWalk`). Open `assets/Scenes/se-world.scene.asset`. Do not use Test or Sponza. Do not use collision QC `root.scene.asset`. Do not use PR 24 / collision product exe. Do not use PR 36 / open-progress apply or PR 37 / VRS as this walk. Budget numbers: Windows 3050; close MCP first.

| # | User story | Pass |
|---|------------|------|
| 1 | Open DogWalk `se-world.scene.asset` in windowed `engine_editor`. The Viewport can show a thin HUD: CPU frame ms, GPU frame ms, main Pass table, instance / light counts. Default off; F3 or Viewport menu toggles it. Off returns today’s clean viewport. This is Layer 1. Opening Tracy is not “editor can see.” | |
| 2 | The same windowed editor can open a **self-drawn Slint dock** (bottom, Animation Window rhythm): frame strip, thread / GPU Pass lanes, click a zone for name and ms. Data is the engine ring. Closing the dock leaves the viewport clean. Not ImGui. Not Tracy View inside the shell. | |
| 3 | The same **dev** build can launch `Tracy.exe` to `127.0.0.1` (default 8086; another port if busy). Sampling starts only when connected; disconnected does not grow to GB. The timeline shows `tickOneFrame` `FrameMark` and Frame graph Pass zones. HUD and dock still have numbers with no connection. The outer window is optional deep dive, not the panel backend. | |
| 4 | Windowed `engine_player` on the same scene has **no** editor dock, but the same Slint HUD (togglable). Title-bar FPS may remain; it is not the only readout. Walk the forest and read frame time from the HUD. Tracy is not required. No Layer 2 dock. | |
| 5 | Shipping / install build: HUD still opens (FPS + frame times + main Passes). The editor dock still binds the ring (read-only). The process does not listen on a profiler port, does not ship `Tracy.exe`, and does not define `TRACY_ENABLE`. | |
| 6 | Headless / `--mcp` / Linux Merge CI have no HUD, no dock, and no Tracy listen. Cloud lavapipe GPU numbers are not acceptance. | |
| 7 | Discrete-GPU machines log the selected `deviceName`. Integrated GPU: HUD warns GPU timings unreliable. The walk is DogWalk `se-world.scene.asset`. Test and Sponza are not the demo. 138 Spot, the dog walking, and a VRS mask are not required. Do not touch PR 24, PR 36, or PR 37. | |
