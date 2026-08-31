# Manual checklist — startup-cover

One row per User story. Agent QC does not substitute. Only the human confirms.

**Status:** Passed 2026-08-31 (human walked windowed `engine_editor`)

Windowed `engine_editor` on a Project with a valid `project.blunder`, unless a row says otherwise.

| # | User story | Pass |
|---|------------|------|
| 1 | Open a windowed Editor Session. First window is Theme + wordmark (Project display name) + English stage name until Editor Shell is on screen — not a black HWND | x |
| 2 | Slow boot (cold cook if needed): stage name changes among Cooking assets / Preparing editor / Starting editor; no percent | x |
| 3 | Close the window while the cover is up. Session ends; no confirm dialog | x |
| 4 | Warm boot: cover may flash; Shell appears as soon as it is ready; no extra wait | x |
| 5 | Headless / CLI / MCP, Project Manager, or Player: no Startup cover | x |
| 6 | Fatal boot while the cover is up (if you can force one): process exits; cover is not an error page and has no Retry | x |
