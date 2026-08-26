## 1. Scene still aspect and Capture

- [x] 1.1 Parameterize Scene still aspect + longest-edge cap; Scene Thumbnail Render stays square and still writes the Thumbnail cache
- [x] 1.2 Capture API beside the still path (not Authorship System): Live `SceneInstance` vs On-disk instantiate; `write_cache` false; Request failure `capture.no_camera` (and no-live / unreadable as needed)
- [x] 1.3 Tests: no Camera fails with no fallback still; successful Capture is 16:9 not square; Capture does not write thumbnail cache; Live includes unsaved pose, On-disk does not
- [x] 1.4 Document `host_observation_test` (or Capture test name) in `docs/agents/testing.md` if it links GPU/SceneInstance

## 2. Play IPC step and frame

- [x] 2.1 Parse/send `step N` and `frame` on the existing Play control channel; NDJSON frame payload (width, height, encoding); no second socket
- [x] 2.2 Extend `play_ipc_test` for those lines; unknown commands stay ignored as today
- [x] 2.3 Playing-state step fails closed (`play.step_requires_pause`); Paused step uses dt 1/60 N times and stays paused

## 3. Player Play frame and editor session

- [x] 3.1 Windowed Player: CPU readback of the Play-rule camera color target (not HWND, not Scene Thumbnail instantiate)
- [x] 3.2 `PlaySessionController` (or hooks) exposes Play step + Play frame to tests; fake-hook tests: step while Playing fails; Pause + step + frame succeeds
- [x] 3.3 Confirm CONTEXT Host observation v1 / ADR 0043 match the shipped API (no extra glossary churn)
- [x] 3.4 Build and run Capture/host-observation tests plus `play_ipc_test` / `play_session_controller_test`
